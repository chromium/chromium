// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_installed_scripts_manager.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/span.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_embedded_worker.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_thread.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

using RawScriptData = ThreadSafeScriptContainer::RawScriptData;

namespace {

// Receiver is a class to read a Mojo data pipe. Received data are stored in
// chunks. Lives on the IO thread. Receiver is owned by Internal via
// BundledReceivers. It is created to read the script body or metadata from a
// data pipe, and is destroyed when the read finishes.
class Receiver {
  DISALLOW_NEW();

 public:
  Receiver(mojo::ScopedDataPipeConsumerHandle handle,
           uint64_t total_bytes,
           scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : handle_(std::move(handle)),
        watcher_(FROM_HERE,
                 mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                 std::move(task_runner)),
        remaining_bytes_(total_bytes) {
    data_.ReserveInitialCapacity(base::checked_cast<wtf_size_t>(total_bytes));
  }

  void Start(base::OnceClosure callback) {
    if (!handle_.is_valid()) {
      std::move(callback).Run();
      return;
    }
    callback_ = std::move(callback);
    // Unretained is safe because |watcher_| is owned by |this|.
    MojoResult rv = watcher_.Watch(
        handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
        WTF::BindRepeating(&Receiver::OnReadable, WTF::Unretained(this)));
    DCHECK_EQ(MOJO_RESULT_OK, rv);
    watcher_.ArmOrNotify();
  }

  void OnReadable(MojoResult) {
    // It isn't necessary to handle MojoResult here since BeginReadDataRaw()
    // returns an equivalent error.
    base::span<const uint8_t> buffer;
    MojoResult rv = handle_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
    switch (rv) {
      case MOJO_RESULT_BUSY:
      case MOJO_RESULT_INVALID_ARGUMENT:
        NOTREACHED_IN_MIGRATION();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        // Closed by peer.
        OnCompleted();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        watcher_.ArmOrNotify();
        return;
      case MOJO_RESULT_OK:
        break;
      default:
        // mojo::BeginReadDataRaw() should not return any other values.
        // Notify the error to the browser by resetting the handle even though
        // it's in the middle of data transfer.
        OnCompleted();
        return;
    }

    if (!buffer.empty()) {
      data_.AppendSpan(base::as_chars(buffer));
    }

    rv = handle_->EndReadData(buffer.size());
    DCHECK_EQ(rv, MOJO_RESULT_OK);
    CHECK_GE(remaining_bytes_, buffer.size());
    remaining_bytes_ -= buffer.size();
    watcher_.ArmOrNotify();
  }

  bool IsRunning() const { return handle_.is_valid(); }
  bool HasReceivedAllData() const { return remaining_bytes_ == 0; }

  Vector<uint8_t> TakeData() {
    DCHECK(!IsRunning());
    return std::move(data_);
  }

 private:
  void OnCompleted() {
    handle_.reset();
    watcher_.Cancel();
    if (!HasReceivedAllData())
      data_.clear();
    DCHECK(callback_);
    std::move(callback_).Run();
  }

  base::OnceClosure callback_;
  mojo::ScopedDataPipeConsumerHandle handle_;
  mojo::SimpleWatcher watcher_;

  Vector<uint8_t> data_;
  uint64_t remaining_bytes_;
};

// BundledReceivers is a helper class to wait for the end of reading body and
// meta data. Lives on the IO thread.
class BundledReceivers {
 public:
  BundledReceivers(mojo::ScopedDataPipeConsumerHandle meta_data_handle,
                   uint64_t meta_data_size,
                   mojo::ScopedDataPipeConsumerHandle body_handle,
                   uint64_t body_size,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : meta_data_(std::move(meta_data_handle), meta_data_size, task_runner),
        body_(std::move(body_handle), body_size, std::move(task_runner)) {}

  // Starts reading the pipes and invokes |callback| when both are finished.
  void Start(base::OnceClosure callback) {
    base::RepeatingClosure wait_all_closure =
        base::BarrierClosure(2, std::move(callback));
    meta_data_.Start(wait_all_closure);
    body_.Start(wait_all_closure);
  }

  Receiver* meta_data() { return &meta_data_; }
  Receiver* body() { return &body_; }

 private:
  Receiver meta_data_;
  Receiver body_;
};

// Internal lives on the IO thread. This receives
// mojom::blink::ServiceWorkerScriptInfo for all installed scripts and then
// starts reading the body and meta data from the browser. This instance will be
// kept alive as long as the Mojo's connection is established.
class Internal : public mojom::blink::ServiceWorkerInstalledScriptsManager {
 public:
  // Called on the IO thread.
  // Creates and binds a new Internal instance to |receiver|.
  static void Create(
      scoped_refptr<ThreadSafeScriptContainer> script_container,
      mojo::PendingReceiver<mojom::blink::ServiceWorkerInstalledScriptsManager>
          receiver,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<Internal>(std::move(script_container),
                                   std::move(task_runner)),
        std::move(receiver));
  }

  Internal(scoped_refptr<ThreadSafeScriptContainer> script_container,
           scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : script_container_(std::move(script_container)),
        task_runner_(std::move(task_runner)) {}

  ~Internal() override {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    // Wake up a waiting thread so it does not wait forever. If the script has
    // not been added yet, that means something went wrong. From here,
    // script_container_->Wait() will return false if the script hasn't been
    // added yet.
    script_container_->OnAllDataAddedOnIOThread();
  }

  // Implements mojom::blink::ServiceWorkerInstalledScriptsManager.
  // Called on the IO thread.
  void TransferInstalledScript(
      mojom::blink::ServiceWorkerScriptInfoPtr script_info) override {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    KURL script_url(script_info->script_url);
    auto receivers = std::make_unique<BundledReceivers>(
        std::move(script_info->meta_data), script_info->meta_data_size,
        std::move(script_info->body), script_info->body_size, task_runner_);
    receivers->Start(WTF::BindOnce(&Internal::OnScriptReceived,
                                   weak_factory_.GetWeakPtr(),
                                   std::move(script_info)));
    DCHECK(!running_receivers_.Contains(script_url));
    running_receivers_.insert(script_url, std::move(receivers));
  }

  // Called on the IO thread.
  void OnScriptReceived(mojom::blink::ServiceWorkerScriptInfoPtr script_info) {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    auto iter = running_receivers_.find(script_info->script_url);
    CHECK(iter != running_receivers_.end(), base::NotFatalUntil::M130);
    std::unique_ptr<BundledReceivers> receivers = std::move(iter->value);
    DCHECK(receivers);
    if (!receivers->body()->HasReceivedAllData() ||
        !receivers->meta_data()->HasReceivedAllData()) {
      script_container_->AddOnIOThread(script_info->script_url,
                                       nullptr /* data */);
      running_receivers_.erase(iter);
      return;
    }

    auto script_data = std::make_unique<RawScriptData>(
        script_info->encoding, receivers->body()->TakeData(),
        receivers->meta_data()->TakeData());
    for (const auto& entry : script_info->headers)
      script_data->AddHeader(entry.key, entry.value);
    script_container_->AddOnIOThread(script_info->script_url,
                                     std::move(script_data));
    running_receivers_.erase(iter);
  }

 private:
  THREAD_CHECKER(io_thread_checker_);
  HashMap<KURL, std::unique_ptr<BundledReceivers>> running_receivers_;
  scoped_refptr<ThreadSafeScriptContainer> script_container_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<Internal> weak_factory_{this};
};

std::unique_ptr<TracedValue> UrlToTracedValue(const KURL& url) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("url", url.GetString());
  return value;
}

}  // namespace

ServiceWorkerInstalledScriptsManager::ServiceWorkerInstalledScriptsManager(
    std::unique_ptr<WebServiceWorkerInstalledScriptsManagerParams>
        installed_scripts_manager_params,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : script_container_(base::MakeRefCounted<ThreadSafeScriptContainer>()) {
  DCHECK(installed_scripts_manager_params);

  DCHECK(installed_scripts_manager_params->manager_receiver);
  auto manager_receiver =
      mojo::PendingReceiver<mojom::blink::ServiceWorkerInstalledScriptsManager>(
          std::move(installed_scripts_manager_params->manager_receiver));

  DCHECK(installed_scripts_manager_params->manager_host_remote);
  manager_host_ = mojo::SharedRemote<
      mojom::blink::ServiceWorkerInstalledScriptsManagerHost>(
      std::move(installed_scripts_manager_params->manager_host_remote));

  // Don't touch |installed_urls_| after this point. We're on the initiator
  // thread now, but |installed_urls_| will be accessed on the
  // worker thread later, so they should keep isolated from the current thread.
  for (const WebURL& url :
       installed_scripts_manager_params->installed_scripts_urls) {
    installed_urls_.insert(KURL(url));
  }

  PostCrossThreadTask(
      *io_task_runner, FROM_HERE,
      CrossThreadBindOnce(&Internal::Create, script_container_,
                          std::move(manager_receiver), io_task_runner));
}

bool ServiceWorkerInstalledScriptsManager::IsScriptInstalled(
    const KURL& script_url) const {
  return installed_urls_.Contains(script_url);
}

std::unique_ptr<InstalledScriptsManager::ScriptData>
ServiceWorkerInstalledScriptsManager::GetScriptData(const KURL& script_url) {
  DCHECK(!IsMainThread());
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerInstalledScriptsManager::GetScriptData",
               "script_url", UrlToTracedValue(script_url));
  if (!IsScriptInstalled(script_url))
    return nullptr;

  // This blocks until the script is received from the browser.
  std::unique_ptr<RawScriptData> raw_script_data = GetRawScriptData(script_url);
  if (!raw_script_data)
    return nullptr;

  // This is from WorkerClassicScriptLoader::DidReceiveData.
  std::unique_ptr<TextResourceDecoder> decoder =
      std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
          TextResourceDecoderOptions::kPlainTextContent,
          raw_script_data->Encoding().empty()
              ? UTF8Encoding()
              : WTF::TextEncoding(raw_script_data->Encoding())));

  Vector<uint8_t> source_text = raw_script_data->TakeScriptText();
  String decoded_source_text = decoder->Decode(base::span(source_text));

  // TODO(crbug.com/946676): Remove the unique_ptr<> wrapper around the Vector
  // as we can just use Vector::IsEmpty() to distinguish missing code cache.
  std::unique_ptr<Vector<uint8_t>> meta_data;
  Vector<uint8_t> meta_data_in = raw_script_data->TakeMetaData();
  if (meta_data_in.size() > 0)
    meta_data = std::make_unique<Vector<uint8_t>>(std::move(meta_data_in));

  return std::make_unique<InstalledScriptsManager::ScriptData>(
      script_url, decoded_source_text, std::move(meta_data),
      raw_script_data->TakeHeaders());
}

std::unique_ptr<RawScriptData>
ServiceWorkerInstalledScriptsManager::GetRawScriptData(const KURL& script_url) {
  ThreadSafeScriptContainer::ScriptStatus status =
      script_container_->GetStatusOnWorkerThread(script_url);
  // If the script has already been taken, request the browser to send the
  // script.
  if (status == ThreadSafeScriptContainer::ScriptStatus::kTaken) {
    script_container_->ResetOnWorkerThread(script_url);
    manager_host_->RequestInstalledScript(script_url);
    status = script_container_->GetStatusOnWorkerThread(script_url);
  }

  // If the script has not been received at this point, wait for arrival by
  // blocking the worker thread.
  if (status == ThreadSafeScriptContainer::ScriptStatus::kPending) {
    // Wait for arrival of the script.
    const bool success = script_container_->WaitOnWorkerThread(script_url);
    // It can fail due to an error on Mojo pipes.
    if (!success)
      return nullptr;
    status = script_container_->GetStatusOnWorkerThread(script_url);
    DCHECK_NE(ThreadSafeScriptContainer::ScriptStatus::kPending, status);
  }

  if (status == ThreadSafeScriptContainer::ScriptStatus::kFailed)
    return nullptr;
  DCHECK_EQ(ThreadSafeScriptContainer::ScriptStatus::kReceived, status);

  return script_container_->TakeOnWorkerThread(script_url);
}

}  // namespace blink
