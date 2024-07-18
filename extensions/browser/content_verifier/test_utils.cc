// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/test_utils.h"

#include "base/base64url.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "crypto/signature_creator.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

namespace extensions {

// TestContentVerifySingleJobObserver ------------------------------------------
TestContentVerifySingleJobObserver::TestContentVerifySingleJobObserver(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path)
    : client_(
          base::MakeRefCounted<ObserverClient>(extension_id, relative_path)) {
  ContentVerifyJob::SetObserverForTests(client_);
}

TestContentVerifySingleJobObserver::~TestContentVerifySingleJobObserver() {
  ContentVerifyJob::SetObserverForTests(nullptr);
}

ContentVerifyJob::FailureReason
TestContentVerifySingleJobObserver::WaitForJobFinished() {
  return client_->WaitForJobFinished();
}

ContentHashReader::InitStatus
TestContentVerifySingleJobObserver::WaitForOnHashesReady() {
  return client_->WaitForOnHashesReady();
}

TestContentVerifySingleJobObserver::ObserverClient::ObserverClient(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path)
    : extension_id_(extension_id), relative_path_(relative_path) {
  EXPECT_TRUE(
      content::BrowserThread::GetCurrentThreadIdentifier(&creation_thread_));
}

TestContentVerifySingleJobObserver::ObserverClient::~ObserverClient() = default;

void TestContentVerifySingleJobObserver::ObserverClient::JobFinished(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path,
    ContentVerifyJob::FailureReason reason) {
  if (!content::BrowserThread::CurrentlyOn(creation_thread_)) {
    content::BrowserThread::GetTaskRunnerForThread(creation_thread_)
        ->PostTask(FROM_HERE,
                   base::BindOnce(&TestContentVerifySingleJobObserver::
                                      ObserverClient::JobFinished,
                                  this, extension_id, relative_path, reason));
    return;
  }
  if (extension_id != extension_id_ || relative_path != relative_path_)
    return;
  EXPECT_FALSE(failure_reason_.has_value());
  failure_reason_ = reason;
  job_finished_run_loop_.Quit();
}

void TestContentVerifySingleJobObserver::ObserverClient::OnHashesReady(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path,
    const ContentHashReader& hash_reader) {
  if (content::BrowserThread::CurrentlyOn(creation_thread_))
    OnHashesReadyOnCreationThread(extension_id, relative_path,
                                  hash_reader.status());
  else {
    content::BrowserThread::GetTaskRunnerForThread(creation_thread_)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&TestContentVerifySingleJobObserver::ObserverClient::
                               OnHashesReadyOnCreationThread,
                           this, extension_id, relative_path,
                           hash_reader.status()));
  }
}

void TestContentVerifySingleJobObserver::ObserverClient::
    OnHashesReadyOnCreationThread(const ExtensionId& extension_id,
                                  const base::FilePath& relative_path,
                                  ContentHashReader::InitStatus hashes_status) {
  if (extension_id != extension_id_ || relative_path != relative_path_)
    return;
  EXPECT_FALSE(seen_on_hashes_ready_);
  seen_on_hashes_ready_ = true;
  hashes_status_ = hashes_status;
  on_hashes_ready_run_loop_.Quit();
}

ContentVerifyJob::FailureReason
TestContentVerifySingleJobObserver::ObserverClient::WaitForJobFinished() {
  // Run() returns immediately if Quit() has already been called.
  job_finished_run_loop_.Run();
  EXPECT_TRUE(failure_reason_.has_value());
  return failure_reason_.value_or(ContentVerifyJob::FAILURE_REASON_MAX);
}

ContentHashReader::InitStatus
TestContentVerifySingleJobObserver::ObserverClient::WaitForOnHashesReady() {
  // Run() returns immediately if Quit() has already been called.
  on_hashes_ready_run_loop_.Run();
  return hashes_status_;
}

// TestContentVerifyJobObserver ------------------------------------------------
TestContentVerifyJobObserver::TestContentVerifyJobObserver()
    : client_(base::MakeRefCounted<ObserverClient>()) {
  ContentVerifyJob::SetObserverForTests(client_);
}

TestContentVerifyJobObserver::~TestContentVerifyJobObserver() {
  ContentVerifyJob::SetObserverForTests(nullptr);
}

void TestContentVerifyJobObserver::ExpectJobResult(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path,
    Result expected_result) {
  client_->ExpectJobResult(extension_id, relative_path, expected_result);
}

bool TestContentVerifyJobObserver::WaitForExpectedJobs() {
  return client_->WaitForExpectedJobs();
}

TestContentVerifyJobObserver::ObserverClient::ObserverClient() {
  EXPECT_TRUE(
      content::BrowserThread::GetCurrentThreadIdentifier(&creation_thread_));
}

TestContentVerifyJobObserver::ObserverClient::~ObserverClient() = default;

void TestContentVerifyJobObserver::ObserverClient::JobFinished(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path,
    ContentVerifyJob::FailureReason failure_reason) {
  if (!content::BrowserThread::CurrentlyOn(creation_thread_)) {
    content::BrowserThread::GetTaskRunnerForThread(creation_thread_)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(
                &TestContentVerifyJobObserver::ObserverClient::JobFinished,
                this, extension_id, relative_path, failure_reason));
    return;
  }
  Result result = failure_reason == ContentVerifyJob::NONE ? Result::SUCCESS
                                                           : Result::FAILURE;
  bool found = false;
  for (auto i = expectations_.begin(); i != expectations_.end(); ++i) {
    if (i->extension_id == extension_id && i->path == relative_path &&
        i->result == result) {
      found = true;
      expectations_.erase(i);
      break;
    }
  }
  if (found) {
    if (expectations_.empty() && job_quit_closure_)
      std::move(job_quit_closure_).Run();
  } else {
    LOG(WARNING) << "Ignoring unexpected JobFinished " << extension_id << "/"
                 << relative_path.value()
                 << " failure_reason:" << failure_reason;
  }
}

void TestContentVerifyJobObserver::ObserverClient::ExpectJobResult(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path,
    Result expected_result) {
  expectations_.push_back(
      ExpectedResult(extension_id, relative_path, expected_result));
}

bool TestContentVerifyJobObserver::ObserverClient::WaitForExpectedJobs() {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(creation_thread_));
  if (!expectations_.empty()) {
    base::RunLoop run_loop;
    job_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  return expectations_.empty();
}

// MockContentVerifierDelegate ------------------------------------------------
MockContentVerifierDelegate::MockContentVerifierDelegate()
    : verifier_key_(
          kWebstoreSignaturesPublicKey,
          kWebstoreSignaturesPublicKey + kWebstoreSignaturesPublicKeySize) {}

MockContentVerifierDelegate::~MockContentVerifierDelegate() = default;

ContentVerifierDelegate::VerifierSourceType
MockContentVerifierDelegate::GetVerifierSourceType(const Extension& extension) {
  return verifier_source_type_;
}

ContentVerifierKey MockContentVerifierDelegate::GetPublicKey() {
  DCHECK_EQ(VerifierSourceType::SIGNED_HASHES, verifier_source_type_);
  return verifier_key_;
}

GURL MockContentVerifierDelegate::GetSignatureFetchUrl(
    const ExtensionId& extension_id,
    const base::Version& version) {
  DCHECK_EQ(VerifierSourceType::SIGNED_HASHES, verifier_source_type_);
  std::string url =
      base::StringPrintf("http://localhost/getsignature?id=%s&version=%s",
                         extension_id.c_str(), version.GetString().c_str());
  return GURL(url);
}

std::set<base::FilePath> MockContentVerifierDelegate::GetBrowserImagePaths(
    const extensions::Extension* extension) {
  return std::set<base::FilePath>();
}

void MockContentVerifierDelegate::VerifyFailed(
    const ExtensionId& extension_id,
    ContentVerifyJob::FailureReason reason) {
  ADD_FAILURE() << "Unexpected call for this test";
}

void MockContentVerifierDelegate::Shutdown() {}

void MockContentVerifierDelegate::SetVerifierSourceType(
    VerifierSourceType type) {
  verifier_source_type_ = type;
}

void MockContentVerifierDelegate::SetVerifierKey(std::vector<uint8_t> key) {
  verifier_key_ = std::move(key);
}

// VerifierObserver -----------------------------------------------------------
VerifierObserver::VerifierObserver() {
  EXPECT_TRUE(
      content::BrowserThread::GetCurrentThreadIdentifier(&creation_thread_));
  ContentVerifier::SetObserverForTests(this);
}

VerifierObserver::~VerifierObserver() {
  ContentVerifier::SetObserverForTests(nullptr);
}

void VerifierObserver::EnsureFetchCompleted(const ExtensionId& extension_id) {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(creation_thread_));
  if (base::Contains(completed_fetches_, extension_id))
    return;
  EXPECT_TRUE(id_to_wait_for_.empty());
  EXPECT_EQ(loop_runner_.get(), nullptr);
  id_to_wait_for_ = extension_id;
  loop_runner_ = base::MakeRefCounted<content::MessageLoopRunner>();
  loop_runner_->Run();
  id_to_wait_for_.clear();
  loop_runner_ = nullptr;
}

void VerifierObserver::OnFetchComplete(
    const scoped_refptr<const ContentHash>& content_hash,
    bool did_hash_mismatch) {
  if (!content::BrowserThread::CurrentlyOn(creation_thread_)) {
    content::BrowserThread::GetTaskRunnerForThread(creation_thread_)
        ->PostTask(FROM_HERE, base::BindOnce(&VerifierObserver::OnFetchComplete,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             content_hash, did_hash_mismatch));
    return;
  }
  const ExtensionId extension_id = content_hash->extension_id();
  completed_fetches_.insert(extension_id);
  content_hash_ = content_hash;
  did_hash_mismatch_ = did_hash_mismatch;
  if (extension_id == id_to_wait_for_) {
    DCHECK(loop_runner_);
    loop_runner_->Quit();
  }
}

// ContentHashResult ----------------------------------------------------------
ContentHashResult::ContentHashResult(
    const ExtensionId& extension_id,
    bool success,
    bool was_cancelled,
    const std::set<base::FilePath> mismatch_paths)
    : extension_id(extension_id),
      success(success),
      was_cancelled(was_cancelled),
      mismatch_paths(mismatch_paths) {}
ContentHashResult::~ContentHashResult() = default;

// ContentHashWaiter ----------------------------------------------------------
ContentHashWaiter::ContentHashWaiter()
    : reply_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}
ContentHashWaiter::~ContentHashWaiter() = default;

std::unique_ptr<ContentHashResult> ContentHashWaiter::CreateAndWaitForCallback(
    ContentHash::FetchKey key,
    ContentVerifierDelegate::VerifierSourceType source_type) {
  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ContentHashWaiter::CreateContentHash,
                     base::Unretained(this), std::move(key), source_type));
  run_loop_.Run();
  DCHECK(result_);
  return std::move(result_);
}

void ContentHashWaiter::CreatedCallback(scoped_refptr<ContentHash> content_hash,
                                        bool was_cancelled) {
  if (!reply_task_runner_->RunsTasksInCurrentSequence()) {
    reply_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ContentHashWaiter::CreatedCallback,
                       base::Unretained(this), content_hash, was_cancelled));
    return;
  }

  DCHECK(content_hash);
  result_ = std::make_unique<ContentHashResult>(
      content_hash->extension_id(), content_hash->succeeded(), was_cancelled,
      content_hash->hash_mismatch_unix_paths());

  run_loop_.QuitWhenIdle();
}

void ContentHashWaiter::CreateContentHash(
    ContentHash::FetchKey key,
    ContentVerifierDelegate::VerifierSourceType source_type) {
  ContentHash::Create(std::move(key), source_type, IsCancelledCallback(),
                      base::BindOnce(&ContentHashWaiter::CreatedCallback,
                                     base::Unretained(this)));
}

namespace content_verifier_test_utils {

// TestExtensionBuilder -------------------------------------------------------
TestExtensionBuilder::TestExtensionBuilder()
    // We have to provide explicit extension id in verified_contents.json.
    : TestExtensionBuilder(ExtensionId(32, 'a')) {}

TestExtensionBuilder::TestExtensionBuilder(const ExtensionId& extension_id)
    : test_content_verifier_key_(crypto::RSAPrivateKey::Create(2048)),
      extension_id_(extension_id) {
  base::CreateDirectory(extension_dir_.UnpackedPath().Append(kMetadataFolder));
}

TestExtensionBuilder::~TestExtensionBuilder() = default;

void TestExtensionBuilder::WriteManifest() {
  extension_dir_.WriteManifest(base::Value::Dict()
                                   .Set("manifest_version", 2)
                                   .Set("name", "Test extension")
                                   .Set("version", "1.0"));
}

void TestExtensionBuilder::WriteResource(
    base::FilePath::StringType relative_path,
    std::string contents) {
  extension_dir_.WriteFile(relative_path, contents);
  AddResource(std::move(relative_path), std::move(contents));
}

void TestExtensionBuilder::AddResource(base::FilePath::StringType relative_path,
                                       std::string contents) {
  extension_resources_.emplace_back(base::FilePath(std::move(relative_path)),
                                    std::move(contents));
}

void TestExtensionBuilder::WriteComputedHashes() {
  int block_size = extension_misc::kContentVerificationDefaultBlockSize;
  ComputedHashes::Data computed_hashes_data;

  for (const auto& resource : extension_resources_) {
    std::vector<std::string> hashes =
        ComputedHashes::GetHashesForContent(resource.contents, block_size);
    computed_hashes_data.Add(resource.relative_path, block_size, hashes);
  }

  ASSERT_TRUE(ComputedHashes(std::move(computed_hashes_data))
                  .WriteToFile(file_util::GetComputedHashesPath(
                      extension_dir_.UnpackedPath())));
}

std::string TestExtensionBuilder::CreateVerifiedContents() const {
  std::unique_ptr<base::Value> payload = CreateVerifiedContentsPayload();
  std::string payload_value;
  if (!base::JSONWriter::Write(*payload, &payload_value))
    return "";

  std::string payload_b64;
  base::Base64UrlEncode(
      payload_value, base::Base64UrlEncodePolicy::OMIT_PADDING, &payload_b64);

  std::string signature_sha256 = crypto::SHA256HashString("." + payload_b64);
  std::vector<uint8_t> signature_source(signature_sha256.begin(),
                                        signature_sha256.end());
  std::vector<uint8_t> signature_value;
  if (!crypto::SignatureCreator::Sign(
          test_content_verifier_key_.get(), crypto::SignatureCreator::SHA256,
          signature_source.data(), signature_source.size(), &signature_value))
    return "";

  std::string signature_b64;
  base::Base64UrlEncode(
      std::string(signature_value.begin(), signature_value.end()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &signature_b64);

  base::Value::List signatures = base::Value::List().Append(
      base::Value::Dict()
          .Set("header", base::Value::Dict().Set("kid", "webstore"))
          .Set("protected", "")
          .Set("signature", signature_b64));
  base::Value::List verified_contents = base::Value::List().Append(
      base::Value::Dict()
          .Set("description", "treehash per file")
          .Set("signed_content",
               base::Value::Dict()
                   .Set("payload", payload_b64)
                   .Set("signatures", std::move(signatures))));

  std::string json;
  if (!base::JSONWriter::Write(verified_contents, &json)) {
    return "";
  }

  return json;
}

void TestExtensionBuilder::WriteVerifiedContents() {
  std::string verified_contents = CreateVerifiedContents();
  ASSERT_NE(verified_contents.size(), 0u);

  base::FilePath verified_contents_path =
      file_util::GetVerifiedContentsPath(extension_dir_.UnpackedPath());
  ASSERT_TRUE(base::WriteFile(verified_contents_path, verified_contents));
}

std::vector<uint8_t> TestExtensionBuilder::GetTestContentVerifierPublicKey()
    const {
  std::vector<uint8_t> public_key;
  test_content_verifier_key_->ExportPublicKey(&public_key);
  return public_key;
}

std::unique_ptr<base::Value>
TestExtensionBuilder::CreateVerifiedContentsPayload() const {
  int block_size = extension_misc::kContentVerificationDefaultBlockSize;

  base::Value::List files;
  for (const auto& resource : extension_resources_) {
    std::string path = base::FilePath(resource.relative_path).AsUTF8Unsafe();
    std::string tree_hash =
        ContentHash::ComputeTreeHashForContent(resource.contents, block_size);

    std::string tree_hash_b64;
    base::Base64UrlEncode(tree_hash, base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &tree_hash_b64);

    files.Append(
        base::Value::Dict().Set("path", path).Set("root_hash", tree_hash_b64));
  }

  base::Value::Dict result =
      base::Value::Dict()
          .Set("item_id", extension_id_)
          .Set("item_version", "1.0")
          .Set("content_hashes", base::Value::List().Append(
                                     base::Value::Dict()
                                         .Set("format", "treehash")
                                         .Set("block_size", block_size)
                                         .Set("hash_block_size", block_size)
                                         .Set("files", std::move(files))));

  return std::make_unique<base::Value>(std::move(result));
}

// Other stuff ----------------------------------------------------------------
scoped_refptr<Extension> UnzipToDirAndLoadExtension(
    const base::FilePath& extension_zip,
    const base::FilePath& unzip_dir) {
  if (!zip::Unzip(extension_zip, unzip_dir)) {
    ADD_FAILURE() << "Failed to unzip path.";
    return nullptr;
  }
  std::string error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      unzip_dir, mojom::ManifestLocation::kInternal, 0 /* flags */, &error);
  EXPECT_NE(nullptr, extension.get()) << " error:'" << error << "'";
  return extension;
}

}  // namespace content_verifier_test_utils

}  // namespace extensions
