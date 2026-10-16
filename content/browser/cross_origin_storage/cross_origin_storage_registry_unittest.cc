// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cross_origin_storage/cross_origin_storage_registry.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_staging_writer.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_types.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "crypto/hash.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/cross_origin_storage/cross_origin_storage.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

CrossOriginStorageHash ComputeHashForString(std::string_view payload) {
  auto digest = crypto::hash::Sha256(base::as_byte_span(payload));
  std::string hex = base::HexEncodeLower(digest);
  auto hash = CrossOriginStorageHash::Create(
      blink::mojom::CrossOriginStorageAlgorithm::kSha256, hex);
  CHECK(hash.has_value());
  return *hash;
}

}  // namespace

TEST(CrossOriginStorageTypesTest, HashCreateValidation) {
  constexpr std::string_view kValidHex =
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

  // Valid 64-character lowercase hex succeeds.
  auto valid = CrossOriginStorageHash::Create(
      blink::mojom::CrossOriginStorageAlgorithm::kSha256, kValidHex);
  ASSERT_TRUE(valid.has_value());
  EXPECT_EQ(valid->algorithm(),
            blink::mojom::CrossOriginStorageAlgorithm::kSha256);
  EXPECT_EQ(valid->value(), kValidHex);

  // Wrong length fails (empty, off-by-one short/long, and non-64).
  EXPECT_FALSE(CrossOriginStorageHash::Create(
                   blink::mojom::CrossOriginStorageAlgorithm::kSha256, "")
                   .has_value());
  EXPECT_FALSE(CrossOriginStorageHash::Create(
                   blink::mojom::CrossOriginStorageAlgorithm::kSha256, "ba7816")
                   .has_value());
  EXPECT_FALSE(CrossOriginStorageHash::Create(
                   blink::mojom::CrossOriginStorageAlgorithm::kSha256,
                   std::string(63, 'a'))
                   .has_value());
  EXPECT_FALSE(CrossOriginStorageHash::Create(
                   blink::mojom::CrossOriginStorageAlgorithm::kSha256,
                   std::string(65, 'a'))
                   .has_value());

  // Uppercase hex characters are rejected.
  std::string uppercase_hex(kValidHex);
  uppercase_hex[0] = 'B';
  EXPECT_FALSE(
      CrossOriginStorageHash::Create(
          blink::mojom::CrossOriginStorageAlgorithm::kSha256, uppercase_hex)
          .has_value());

  // Non-hex ASCII boundary characters ('/' before '0', ':' after '9',
  // '`' before 'a', 'g' after 'f') are rejected.
  for (char invalid_char : {'/', ':', '`', 'g'}) {
    std::string non_hex(kValidHex);
    non_hex[63] = invalid_char;
    EXPECT_FALSE(
        CrossOriginStorageHash::Create(
            blink::mojom::CrossOriginStorageAlgorithm::kSha256, non_hex)
            .has_value());
  }
}

TEST(CrossOriginStorageTypesTest, HashMatchesDigest) {
  constexpr std::string_view kPayload = "abc";
  CrossOriginStorageHash hash = ComputeHashForString(kPayload);

  auto matching_digest = crypto::hash::Sha256(base::as_byte_span(kPayload));
  EXPECT_TRUE(hash.MatchesDigest(matching_digest));

  auto other_digest =
      crypto::hash::Sha256(base::as_byte_span(std::string_view("xyz")));
  EXPECT_FALSE(hash.MatchesDigest(other_digest));

  // Wrong digest lengths are rejected.
  EXPECT_FALSE(hash.MatchesDigest({}));
  EXPECT_FALSE(hash.MatchesDigest(
      base::span(matching_digest).first(crypto::hash::kSha256Size - 1)));
  std::vector<uint8_t> oversized_digest(matching_digest.begin(),
                                        matching_digest.end());
  oversized_digest.push_back(0);
  EXPECT_FALSE(hash.MatchesDigest(oversized_digest));
}

TEST(CrossOriginStorageTypesTest, AccessSpecFromMojo) {
  // Wildcard target_origins sets any_origin = true.
  auto wildcard_union = blink::mojom::CrossOriginStorageOrigins::NewAnyOrigin(
      blink::mojom::CrossOriginStorageAnyOrigin::New());
  auto wildcard_spec = CrossOriginStorageAccessSpec::FromMojo(*wildcard_union);
  ASSERT_TRUE(wildcard_spec.has_value());
  EXPECT_TRUE(wildcard_spec->any_origin());
  EXPECT_TRUE(wildcard_spec->allowed_origins().empty());

  // Empty origin list succeeds with default (same-site only) visibility.
  auto empty_union = blink::mojom::CrossOriginStorageOrigins::NewOrigins({});
  auto empty_spec = CrossOriginStorageAccessSpec::FromMojo(*empty_union);
  ASSERT_TRUE(empty_spec.has_value());
  EXPECT_FALSE(empty_spec->any_origin());
  EXPECT_TRUE(empty_spec->allowed_origins().empty());

  // Explicit potentially trustworthy http(s) origin list succeeds (including
  // loopback http).
  url::Origin https_origin =
      url::Origin::Create(GURL("https://cdn.example.com"));
  url::Origin localhost_origin =
      url::Origin::Create(GURL("http://localhost:8080"));
  auto list_union = blink::mojom::CrossOriginStorageOrigins::NewOrigins(
      {https_origin, localhost_origin});
  auto list_spec = CrossOriginStorageAccessSpec::FromMojo(*list_union);
  ASSERT_TRUE(list_spec.has_value());
  EXPECT_FALSE(list_spec->any_origin());
  ASSERT_EQ(list_spec->allowed_origins().size(), 2u);
  EXPECT_EQ(list_spec->allowed_origins()[0], https_origin);
  EXPECT_EQ(list_spec->allowed_origins()[1], localhost_origin);

  // Duplicate origins within `kMaxTargetOriginsLength` succeed and are
  // deduplicated per WICG spec § 4.1 (`normalize requested origins`).
  std::vector<url::Origin> duplicate_origins(
      cos_constants::kMaxTargetOriginsLength, https_origin);
  auto duplicate_union = blink::mojom::CrossOriginStorageOrigins::NewOrigins(
      std::move(duplicate_origins));
  auto duplicate_spec =
      CrossOriginStorageAccessSpec::FromMojo(*duplicate_union);
  ASSERT_TRUE(duplicate_spec.has_value());
  ASSERT_EQ(duplicate_spec->allowed_origins().size(), 1u);
  EXPECT_EQ(duplicate_spec->allowed_origins()[0], https_origin);

  // Exact upper bound (`kMaxTargetOriginsLength` distinct origins) succeeds.
  std::vector<url::Origin> max_origins;
  for (size_t i = 0; i < cos_constants::kMaxTargetOriginsLength; ++i) {
    max_origins.push_back(url::Origin::Create(
        GURL("https://site-" + base::NumberToString(i) + ".test")));
  }
  auto max_union = blink::mojom::CrossOriginStorageOrigins::NewOrigins(
      std::move(max_origins));
  auto max_spec = CrossOriginStorageAccessSpec::FromMojo(*max_union);
  ASSERT_TRUE(max_spec.has_value());
  EXPECT_EQ(max_spec->allowed_origins().size(),
            cos_constants::kMaxTargetOriginsLength);

  // Opaque, non-http(s) (`wss:`, `file:`), and untrustworthy (`http:` on a
  // non-loopback host) origins are rejected.
  for (const url::Origin& invalid_origin :
       {url::Origin(), url::Origin::Create(GURL("http://cdn.example.com")),
        url::Origin::Create(GURL("wss://cdn.example.com")),
        url::Origin::Create(GURL("file:///tmp/test.txt"))}) {
    auto invalid_union =
        blink::mojom::CrossOriginStorageOrigins::NewOrigins({invalid_origin});
    EXPECT_FALSE(
        CrossOriginStorageAccessSpec::FromMojo(*invalid_union).has_value());
  }

  // Exceeding kMaxTargetOriginsLength is rejected.
  std::vector<url::Origin> too_many_origins(
      cos_constants::kMaxTargetOriginsLength + 1, https_origin);
  auto too_many_union = blink::mojom::CrossOriginStorageOrigins::NewOrigins(
      std::move(too_many_origins));
  EXPECT_FALSE(
      CrossOriginStorageAccessSpec::FromMojo(*too_many_union).has_value());
}

class CrossOriginStorageStagingWriterTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  CrossOriginStorageStagingWriter CreateWriter(base::FilePath* out_path) {
    *out_path = temp_dir_.GetPath().AppendASCII("staging_file.tmp");
    base::File file(*out_path, base::File::FLAG_CREATE_ALWAYS |
                                   base::File::FLAG_READ |
                                   base::File::FLAG_WRITE);
    CHECK(file.IsValid());
    return CrossOriginStorageStagingWriter(
        blink::mojom::CrossOriginStorageAlgorithm::kSha256, *out_path,
        std::move(file));
  }

  base::ScopedTempDir temp_dir_;
};

TEST_F(CrossOriginStorageStagingWriterTest, AppendAndFinishMatchesDigest) {
  base::FilePath staging_path;
  CrossOriginStorageStagingWriter::Result result;
  {
    CrossOriginStorageStagingWriter writer = CreateWriter(&staging_path);

    const std::string chunk1 = "hello ";
    const std::string chunk2 = "world";
    EXPECT_EQ(writer.Append(std::vector<uint8_t>(chunk1.begin(), chunk1.end())),
              CrossOriginStorageStatus::kSuccess);
    EXPECT_EQ(writer.Append(std::vector<uint8_t>(chunk2.begin(), chunk2.end())),
              CrossOriginStorageStatus::kSuccess);

    result = writer.Finish();
    // Calling Discard() after Finish() must not delete the finished file since
    // Finish() transfers path ownership to the caller.
    writer.Discard();
  }
  // Destroying `writer` after `Finish()` must preserve the closed staging file
  // on disk so `Commit()` can move it into the cache.
  ASSERT_EQ(result.status, CrossOriginStorageStatus::kSuccess);
  EXPECT_EQ(result.size, 11);
  EXPECT_EQ(result.path, staging_path);
  CrossOriginStorageHash expected = ComputeHashForString("hello world");
  EXPECT_TRUE(expected.MatchesDigest(result.digest));
  EXPECT_TRUE(base::PathExists(staging_path));
}

TEST_F(CrossOriginStorageStagingWriterTest,
       TruncateToZeroResetsHasherAndNoOpTruncatePreservesDigest) {
  base::FilePath staging_path;
  CrossOriginStorageStagingWriter writer = CreateWriter(&staging_path);

  const std::string discarded = "discarded prefix";
  ASSERT_EQ(
      writer.Append(std::vector<uint8_t>(discarded.begin(), discarded.end())),
      CrossOriginStorageStatus::kSuccess);

  // Truncating to 0 resets the streaming hasher without needing a disk rehash.
  ASSERT_EQ(writer.Truncate(0), CrossOriginStorageStatus::kSuccess);

  const std::string final_text = "fresh content";
  ASSERT_EQ(
      writer.Append(std::vector<uint8_t>(final_text.begin(), final_text.end())),
      CrossOriginStorageStatus::kSuccess);
  // No-op truncate to current length preserves the streaming digest.
  ASSERT_EQ(writer.Truncate(final_text.size()),
            CrossOriginStorageStatus::kSuccess);

  CrossOriginStorageStagingWriter::Result result = writer.Finish();
  ASSERT_EQ(result.status, CrossOriginStorageStatus::kSuccess);
  EXPECT_EQ(result.size, static_cast<int64_t>(final_text.size()));
  EXPECT_TRUE(ComputeHashForString(final_text).MatchesDigest(result.digest));
}

TEST_F(CrossOriginStorageStagingWriterTest,
       OutOfOrderWriteAndTruncateRehashesFromDisk) {
  base::FilePath staging_path;
  CrossOriginStorageStagingWriter writer = CreateWriter(&staging_path);

  // Use a >128 KiB payload so `RehashFromDisk()` exercises multiple 64 KiB read
  // iterations after an out-of-order seek and truncate.
  constexpr size_t kFinalSize = 140 * 1024;
  std::string expected_payload(kFinalSize, 'a');
  const std::string patch = "patched-middle-block";
  expected_payload.replace(70 * 1024, patch.size(), patch);

  std::string initial = expected_payload + "trailing-garbage";
  std::fill(initial.begin() + 70 * 1024,
            initial.begin() + 70 * 1024 + patch.size(), 'X');
  ASSERT_EQ(writer.Append(std::vector<uint8_t>(initial.begin(), initial.end())),
            CrossOriginStorageStatus::kSuccess);

  ASSERT_EQ(writer.WriteAtOffset(
                70 * 1024, std::vector<uint8_t>(patch.begin(), patch.end())),
            CrossOriginStorageStatus::kSuccess);

  ASSERT_EQ(writer.Truncate(kFinalSize), CrossOriginStorageStatus::kSuccess);

  CrossOriginStorageStagingWriter::Result result = writer.Finish();
  ASSERT_EQ(result.status, CrossOriginStorageStatus::kSuccess);
  EXPECT_EQ(result.size, static_cast<int64_t>(kFinalSize));
  CrossOriginStorageHash expected = ComputeHashForString(expected_payload);
  EXPECT_TRUE(expected.MatchesDigest(result.digest));
}

TEST_F(CrossOriginStorageStagingWriterTest, MaxSizeEnforcement) {
  base::FilePath staging_path;
  CrossOriginStorageStagingWriter writer = CreateWriter(&staging_path);
  writer.SetMaxSizeBytesForTesting(10);

  const std::string chunk1 = "123456";
  EXPECT_EQ(writer.Append(std::vector<uint8_t>(chunk1.begin(), chunk1.end())),
            CrossOriginStorageStatus::kSuccess);

  // Appending 5 more bytes exceeds the 10-byte limit and reports
  // kQuotaExceeded.
  const std::string chunk2 = "78901";
  EXPECT_EQ(writer.Append(std::vector<uint8_t>(chunk2.begin(), chunk2.end())),
            CrossOriginStorageStatus::kQuotaExceeded);

  CrossOriginStorageStagingWriter::Result result = writer.Finish();
  EXPECT_EQ(result.status, CrossOriginStorageStatus::kQuotaExceeded);

  writer.Discard();
  EXPECT_FALSE(base::PathExists(staging_path));

  // Exceeding the limit via Truncate() also returns kQuotaExceeded.
  CrossOriginStorageStagingWriter truncate_writer = CreateWriter(&staging_path);
  truncate_writer.SetMaxSizeBytesForTesting(10);
  EXPECT_EQ(truncate_writer.Truncate(11),
            CrossOriginStorageStatus::kQuotaExceeded);
}

TEST_F(CrossOriginStorageStagingWriterTest,
       DestructorCleansUpUnfinishedStagingFile) {
  base::FilePath staging_path;
  {
    CrossOriginStorageStagingWriter writer = CreateWriter(&staging_path);
    const std::string chunk = "partial data";
    EXPECT_EQ(writer.Append(std::vector<uint8_t>(chunk.begin(), chunk.end())),
              CrossOriginStorageStatus::kSuccess);
    EXPECT_TRUE(base::PathExists(staging_path));
  }
  // Destroying `writer` without calling Finish() must delete the staging file.
  EXPECT_FALSE(base::PathExists(staging_path));

  // Calling Discard() before Finish() returns kOperationFailed without
  // crashing, and repeated Discard() calls are safe no-ops.
  CrossOriginStorageStagingWriter discarded_writer =
      CreateWriter(&staging_path);
  discarded_writer.Discard();
  CrossOriginStorageStagingWriter::Result discarded_res =
      discarded_writer.Finish();
  EXPECT_EQ(discarded_res.status, CrossOriginStorageStatus::kOperationFailed);
  discarded_writer.Discard();
}

class CrossOriginStorageRegistryTest : public testing::Test {
 protected:
  void SetUp() override {
    browser_context_ = std::make_unique<TestBrowserContext>();
  }

  CrossOriginStorageRegistry* registry() {
    return CrossOriginStorageRegistry::GetOrCreateForBrowserContext(
        browser_context_.get());
  }

  CrossOriginStorageStatus StageAndCommit(
      const url::Origin& caller,
      const CrossOriginStorageHash& claimed_hash,
      std::string_view payload,
      CrossOriginStorageAccessSpec spec) {
    base::test::TestFuture<base::SequenceBound<CrossOriginStorageStagingWriter>>
        create_future;
    registry()->CreateStagingWriter(claimed_hash.algorithm(),
                                    create_future.GetCallback());
    base::SequenceBound<CrossOriginStorageStagingWriter> writer =
        create_future.Take();
    EXPECT_TRUE(writer);

    std::vector<uint8_t> bytes(payload.begin(), payload.end());
    base::test::TestFuture<CrossOriginStorageStatus> append_future;
    writer.AsyncCall(&CrossOriginStorageStagingWriter::Append)
        .WithArgs(std::move(bytes))
        .Then(append_future.GetCallback());
    EXPECT_EQ(append_future.Get(), CrossOriginStorageStatus::kSuccess);

    base::test::TestFuture<CrossOriginStorageStagingWriter::Result>
        finish_future;
    writer.AsyncCall(&CrossOriginStorageStagingWriter::Finish)
        .Then(finish_future.GetCallback());
    CrossOriginStorageStagingWriter::Result write_result = finish_future.Take();
    EXPECT_EQ(write_result.status, CrossOriginStorageStatus::kSuccess);

    base::test::TestFuture<CrossOriginStorageStatus> commit_future;
    registry()->Commit(claimed_hash, caller, std::move(spec),
                       std::move(write_result), commit_future.GetCallback());
    return commit_future.Get();
  }

  CrossOriginStorageOpenResult OpenForRead(const url::Origin& caller,
                                           const CrossOriginStorageHash& hash,
                                           bool update_access_time = true) {
    base::test::TestFuture<CrossOriginStorageOpenResult> read_future;
    registry()->OpenForRead(hash, caller, update_access_time,
                            read_future.GetCallback());
    return read_future.Take();
  }

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;
};

TEST_F(CrossOriginStorageRegistryTest, OffTheRecordBrowserContextReturnsNull) {
  browser_context_->set_is_off_the_record(true);
  ASSERT_FALSE(browser_context_->GetPath().empty());
  EXPECT_EQ(registry(), nullptr);
}

TEST_F(CrossOriginStorageRegistryTest, CommitMatchingAndMismatchedDigest) {
  const url::Origin kOriginA =
      url::Origin::Create(GURL("https://a.example.com"));
  constexpr std::string_view kPayload = "shared wasm module payload";
  const CrossOriginStorageHash kCorrectHash = ComputeHashForString(kPayload);
  const CrossOriginStorageHash kWrongHash =
      ComputeHashForString("different payload");

  CrossOriginStorageAccessSpec spec;

  // Mismatched digest is rejected with kDataError and not stored.
  EXPECT_EQ(StageAndCommit(kOriginA, kWrongHash, kPayload, spec),
            CrossOriginStorageStatus::kDataError);
  {
    CrossOriginStorageOpenResult read_res = OpenForRead(kOriginA, kCorrectHash);
    EXPECT_EQ(read_res.status, CrossOriginStorageStatus::kNotFound);
  }

  // Opaque, non-trustworthy HTTP, and non-HTTP(S) origins are rejected with
  // kInvalidArgument.
  EXPECT_EQ(StageAndCommit(url::Origin(), kCorrectHash, kPayload, spec),
            CrossOriginStorageStatus::kInvalidArgument);
  EXPECT_EQ(
      StageAndCommit(url::Origin::Create(GURL("http://insecure.example.com")),
                     kCorrectHash, kPayload, spec),
      CrossOriginStorageStatus::kInvalidArgument);
  EXPECT_EQ(
      StageAndCommit(url::Origin::Create(
                         GURL("chrome-extension://abcdefghijklmnopqrstuvwxyz")),
                     kCorrectHash, kPayload, spec),
      CrossOriginStorageStatus::kInvalidArgument);

  // Committing a path outside `staging/` fails without deleting the external
  // file, and committing a non-existent path inside `staging/` fails cleanly.
  {
    auto valid_digest = crypto::hash::Sha256(base::as_byte_span(kPayload));
    base::FilePath external_path =
        browser_context_->GetPath().AppendASCII("external_file.dat");
    ASSERT_TRUE(base::WriteFile(external_path, kPayload));

    CrossOriginStorageStagingWriter::Result outside_result;
    outside_result.status = CrossOriginStorageStatus::kSuccess;
    outside_result.path = external_path;
    outside_result.digest =
        std::vector<uint8_t>(valid_digest.begin(), valid_digest.end());

    base::test::TestFuture<CrossOriginStorageStatus> outside_commit;
    registry()->Commit(kCorrectHash, kOriginA, spec, std::move(outside_result),
                       outside_commit.GetCallback());
    EXPECT_EQ(outside_commit.Get(), CrossOriginStorageStatus::kOperationFailed);
    EXPECT_TRUE(base::PathExists(external_path));

    CrossOriginStorageStagingWriter::Result missing_result;
    missing_result.status = CrossOriginStorageStatus::kSuccess;
    missing_result.path = browser_context_->GetPath()
                              .AppendASCII("CrossOriginStorage")
                              .AppendASCII("staging")
                              .AppendASCII("missing.tmp");
    missing_result.digest =
        std::vector<uint8_t>(valid_digest.begin(), valid_digest.end());

    base::test::TestFuture<CrossOriginStorageStatus> missing_commit;
    registry()->Commit(kCorrectHash, kOriginA, spec, std::move(missing_result),
                       missing_commit.GetCallback());
    EXPECT_EQ(missing_commit.Get(), CrossOriginStorageStatus::kOperationFailed);
  }

  // Failed staging result status (e.g. kQuotaExceeded) is preserved by Commit
  // rather than returning a generic kOperationFailed.
  {
    CrossOriginStorageStagingWriter::Result failed_staging_result;
    failed_staging_result.status = CrossOriginStorageStatus::kQuotaExceeded;
    base::test::TestFuture<CrossOriginStorageStatus> failed_commit;
    registry()->Commit(kCorrectHash, kOriginA, spec,
                       std::move(failed_staging_result),
                       failed_commit.GetCallback());
    EXPECT_EQ(failed_commit.Get(), CrossOriginStorageStatus::kQuotaExceeded);
  }

  // Matching digest commits and can be read back by the storing origin.
  EXPECT_EQ(StageAndCommit(kOriginA, kCorrectHash, kPayload, spec),
            CrossOriginStorageStatus::kSuccess);
  {
    CrossOriginStorageOpenResult read_res = OpenForRead(kOriginA, kCorrectHash);
    EXPECT_EQ(read_res.status, CrossOriginStorageStatus::kSuccess);
    EXPECT_EQ(read_res.size, static_cast<int64_t>(kPayload.size()));
    std::string contents;
    ASSERT_TRUE(base::ReadFileToString(read_res.path, &contents));
    EXPECT_EQ(contents, kPayload);
  }
}

TEST_F(CrossOriginStorageRegistryTest,
       AccessControlSameSiteAllowedOriginsAndWildcard) {
  const url::Origin kStoringOrigin =
      url::Origin::Create(GURL("https://app.example.com"));
  const url::Origin kSameSiteOrigin =
      url::Origin::Create(GURL("https://sub.example.com"));
  const url::Origin kAllowedCrossOrigin =
      url::Origin::Create(GURL("https://partner.other.test"));
  const url::Origin kSameSiteAllowedCrossOrigin =
      url::Origin::Create(GURL("https://sub.other.test"));
  const url::Origin kDisallowedCrossOrigin =
      url::Origin::Create(GURL("https://stranger.evil.test"));

  constexpr std::string_view kPayload = "cross-origin asset";
  const CrossOriginStorageHash kHash = ComputeHashForString(kPayload);

  // 1. Commit with default visibility (same-site only).
  CrossOriginStorageAccessSpec default_spec;
  ASSERT_EQ(StageAndCommit(kStoringOrigin, kHash, kPayload, default_spec),
            CrossOriginStorageStatus::kSuccess);

  // Storing origin and same-site origin can read.
  EXPECT_EQ(OpenForRead(kStoringOrigin, kHash).status,
            CrossOriginStorageStatus::kSuccess);
  EXPECT_EQ(OpenForRead(kSameSiteOrigin, kHash).status,
            CrossOriginStorageStatus::kSuccess);

  // Cross-site origins and opaque origins get kNotFound (indistinguishable from
  // missing entry).
  EXPECT_EQ(OpenForRead(kAllowedCrossOrigin, kHash).status,
            CrossOriginStorageStatus::kNotFound);
  EXPECT_EQ(OpenForRead(kSameSiteAllowedCrossOrigin, kHash).status,
            CrossOriginStorageStatus::kNotFound);
  EXPECT_EQ(OpenForRead(url::Origin(), kHash).status,
            CrossOriginStorageStatus::kNotFound);

  // 2. Additive visibility upgrade via deduplicated Commit granting access to
  // kAllowedCrossOrigin (which extends to same-site subdomains per spec).
  auto grant_partner_spec =
      CrossOriginStorageAccessSpec::AllowedOrigins({kAllowedCrossOrigin});
  ASSERT_EQ(StageAndCommit(kStoringOrigin, kHash, kPayload, grant_partner_spec),
            CrossOriginStorageStatus::kSuccess);

  EXPECT_EQ(OpenForRead(kAllowedCrossOrigin, kHash).status,
            CrossOriginStorageStatus::kSuccess);
  EXPECT_EQ(OpenForRead(kSameSiteAllowedCrossOrigin, kHash).status,
            CrossOriginStorageStatus::kSuccess);
  EXPECT_EQ(OpenForRead(kDisallowedCrossOrigin, kHash).status,
            CrossOriginStorageStatus::kNotFound);

  // 3. Deduplicated Commit by another origin with AnyOrigin() upgrades
  // visibility to all non-opaque origins.
  auto wildcard_spec = CrossOriginStorageAccessSpec::AnyOrigin();
  ASSERT_EQ(StageAndCommit(kAllowedCrossOrigin, kHash, kPayload, wildcard_spec),
            CrossOriginStorageStatus::kSuccess);
  EXPECT_EQ(OpenForRead(kDisallowedCrossOrigin, kHash).status,
            CrossOriginStorageStatus::kSuccess);
}

TEST_F(CrossOriginStorageRegistryTest,
       StaleMetadataNotInheritedWhenBlobIsMissingOnDisk) {
  const url::Origin kOriginA =
      url::Origin::Create(GURL("https://a.example.com"));
  const url::Origin kOriginB =
      url::Origin::Create(GURL("https://b.other.test"));
  constexpr std::string_view kPayload = "transient blob";
  const CrossOriginStorageHash kHash = ComputeHashForString(kPayload);

  // Commit with wildcard visibility (`AnyOrigin()`).
  auto wildcard_spec = CrossOriginStorageAccessSpec::AnyOrigin();
  ASSERT_EQ(StageAndCommit(kOriginA, kHash, kPayload, wildcard_spec),
            CrossOriginStorageStatus::kSuccess);

  // Simulate an external deletion of the on-disk blob file while metadata
  // still exists in memory and in metadata.json.
  base::FilePath blob_path = browser_context_->GetPath()
                                 .AppendASCII("CrossOriginStorage")
                                 .AppendASCII("blobs")
                                 .AppendASCII("sha256")
                                 .AppendASCII(kHash.value());
  ASSERT_TRUE(base::DeleteFile(blob_path));

  // Re-committing the same blob with restricted (same-site only) visibility
  // must NOT inherit the stale `any_origin` grant.
  CrossOriginStorageAccessSpec restricted_spec;
  ASSERT_EQ(StageAndCommit(kOriginA, kHash, kPayload, restricted_spec),
            CrossOriginStorageStatus::kSuccess);

  EXPECT_EQ(OpenForRead(kOriginB, kHash).status,
            CrossOriginStorageStatus::kNotFound);
  EXPECT_EQ(OpenForRead(kOriginA, kHash).status,
            CrossOriginStorageStatus::kSuccess);
}

TEST_F(CrossOriginStorageRegistryTest,
       PersistenceAcrossRestartsAndStartupStagingSweep) {
  const url::Origin kOriginB =
      url::Origin::Create(GURL("https://b.other.test"));
  const url::Origin kOriginC =
      url::Origin::Create(GURL("https://c.unrelated.test"));
  const url::Origin kLocalhostOrigin =
      url::Origin::Create(GURL("http://localhost:8080"));
  constexpr std::string_view kPayload = "persisted payload";
  const CrossOriginStorageHash kHash = ComputeHashForString(kPayload);
  constexpr std::string_view kWildcardPayload = "wildcard persisted payload";
  const CrossOriginStorageHash kWildcardHash =
      ComputeHashForString(kWildcardPayload);

  auto commit_spec = CrossOriginStorageAccessSpec::AllowedOrigins({kOriginB});
  ASSERT_EQ(StageAndCommit(kLocalhostOrigin, kHash, kPayload, commit_spec),
            CrossOriginStorageStatus::kSuccess);
  ASSERT_EQ(StageAndCommit(kOriginB, kWildcardHash, kWildcardPayload,
                           CrossOriginStorageAccessSpec::AnyOrigin()),
            CrossOriginStorageStatus::kSuccess);

  // Place an orphaned file in the staging directory and an unindexed file in
  // the blobs directory to simulate an interrupted write prior to restart.
  base::FilePath orphan_staging_file = browser_context_->GetPath()
                                           .AppendASCII("CrossOriginStorage")
                                           .AppendASCII("staging")
                                           .AppendASCII("orphan.tmp");
  ASSERT_TRUE(base::WriteFile(orphan_staging_file, "interrupted write"));
  ASSERT_TRUE(base::PathExists(orphan_staging_file));

  base::FilePath orphan_blob_file = browser_context_->GetPath()
                                        .AppendASCII("CrossOriginStorage")
                                        .AppendASCII("blobs")
                                        .AppendASCII("sha256")
                                        .AppendASCII(
                                            "ffffffffffffffffffffffffffffffffff"
                                            "ffffffffffffffffffffffffffffff");
  ASSERT_TRUE(base::WriteFile(orphan_blob_file, "unindexed blob"));
  ASSERT_TRUE(base::PathExists(orphan_blob_file));

  // Destroy the current BrowserContext (and its registry) after synchronously
  // flushing the backend shutdown on its sequence while preserving the profile
  // directory on disk, then recreate a new BrowserContext pointed at the same
  // directory.
  registry()->SynchronouslyResetBackendForTesting();
  base::FilePath saved_profile_path = browser_context_->TakePath();
  browser_context_.reset();
  browser_context_ =
      std::make_unique<TestBrowserContext>(std::move(saved_profile_path));

  // Trigger registry initialization and verify that storing_origins (including
  // loopback http origins), allowed_origins, and any_origin all survived
  // reload, and both orphaned staging and unindexed blob files were swept.
  EXPECT_EQ(OpenForRead(kLocalhostOrigin, kHash).status,
            CrossOriginStorageStatus::kSuccess);
  EXPECT_EQ(OpenForRead(kOriginC, kWildcardHash).status,
            CrossOriginStorageStatus::kSuccess);
  CrossOriginStorageOpenResult read_res = OpenForRead(kOriginB, kHash);
  EXPECT_EQ(read_res.status, CrossOriginStorageStatus::kSuccess);
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(read_res.path, &contents));
  EXPECT_EQ(contents, kPayload);
  EXPECT_FALSE(base::PathExists(orphan_staging_file));
  EXPECT_FALSE(base::PathExists(orphan_blob_file));
}

TEST_F(CrossOriginStorageRegistryTest,
       CorruptOrIncompatibleMetadataRecoversCleanly) {
  const url::Origin kOriginA =
      url::Origin::Create(GURL("https://a.example.com"));
  constexpr std::string_view kPayload = "payload before corruption";
  const CrossOriginStorageHash kHash = ComputeHashForString(kPayload);

  ASSERT_EQ(
      StageAndCommit(kOriginA, kHash, kPayload, CrossOriginStorageAccessSpec()),
      CrossOriginStorageStatus::kSuccess);

  const base::FilePath metadata_path = browser_context_->GetPath()
                                           .AppendASCII("CrossOriginStorage")
                                           .AppendASCII("metadata.json");
  const base::FilePath blob_path = browser_context_->GetPath()
                                       .AppendASCII("CrossOriginStorage")
                                       .AppendASCII("blobs")
                                       .AppendASCII("sha256")
                                       .AppendASCII(kHash.value());

  // 1. Corrupt JSON in metadata.json sweeps existing unindexed blobs and allows
  // subsequent commits to succeed cleanly.
  registry()->SynchronouslyResetBackendForTesting();
  base::FilePath saved_profile_path = browser_context_->TakePath();
  browser_context_.reset();
  ASSERT_TRUE(base::WriteFile(metadata_path, "{corrupt json"));
  ASSERT_TRUE(base::PathExists(blob_path));

  browser_context_ =
      std::make_unique<TestBrowserContext>(std::move(saved_profile_path));
  EXPECT_EQ(OpenForRead(kOriginA, kHash).status,
            CrossOriginStorageStatus::kNotFound);
  EXPECT_FALSE(base::PathExists(blob_path));
  EXPECT_EQ(
      StageAndCommit(kOriginA, kHash, kPayload, CrossOriginStorageAccessSpec()),
      CrossOriginStorageStatus::kSuccess);

  // 2. Incompatible version in metadata.json likewise starts clean and sweeps
  // unindexed blobs.
  registry()->SynchronouslyResetBackendForTesting();
  saved_profile_path = browser_context_->TakePath();
  browser_context_.reset();
  ASSERT_TRUE(
      base::WriteFile(metadata_path, R"({"version": 999, "entries": {}})"));
  ASSERT_TRUE(base::PathExists(blob_path));

  browser_context_ =
      std::make_unique<TestBrowserContext>(std::move(saved_profile_path));
  EXPECT_EQ(OpenForRead(kOriginA, kHash).status,
            CrossOriginStorageStatus::kNotFound);
  EXPECT_FALSE(base::PathExists(blob_path));
}

TEST_F(CrossOriginStorageRegistryTest,
       ConcurrentStagingWritersAndAbortedWritesLeaveNoStaleEntries) {
  const url::Origin kOriginA =
      url::Origin::Create(GURL("https://a.example.com"));
  const url::Origin kOriginB =
      url::Origin::Create(GURL("https://b.other.test"));
  const url::Origin kOriginC =
      url::Origin::Create(GURL("https://c.partner.test"));
  constexpr std::string_view kPayload = "concurrent payload";
  const CrossOriginStorageHash kHash = ComputeHashForString(kPayload);

  // Open two concurrent staging writers for the same hash.
  base::test::TestFuture<base::SequenceBound<CrossOriginStorageStagingWriter>>
      create_future_1;
  base::test::TestFuture<base::SequenceBound<CrossOriginStorageStagingWriter>>
      create_future_2;
  registry()->CreateStagingWriter(kHash.algorithm(),
                                  create_future_1.GetCallback());
  registry()->CreateStagingWriter(kHash.algorithm(),
                                  create_future_2.GetCallback());

  base::SequenceBound<CrossOriginStorageStagingWriter> writer_1 =
      create_future_1.Take();
  base::SequenceBound<CrossOriginStorageStagingWriter> writer_2 =
      create_future_2.Take();
  ASSERT_TRUE(writer_1);
  ASSERT_TRUE(writer_2);

  // Writer 1 writes partial data and aborts; Writer 2 writes full data and
  // commits for kOriginB (restricted visibility).
  const std::string partial = "partial";
  base::test::TestFuture<CrossOriginStorageStatus> append_future_1;
  writer_1.AsyncCall(&CrossOriginStorageStagingWriter::Append)
      .WithArgs(std::vector<uint8_t>(partial.begin(), partial.end()))
      .Then(append_future_1.GetCallback());
  ASSERT_EQ(append_future_1.Get(), CrossOriginStorageStatus::kSuccess);

  base::test::TestFuture<void> discard_future_1;
  writer_1.AsyncCall(&CrossOriginStorageStagingWriter::Discard)
      .Then(discard_future_1.GetCallback());
  ASSERT_TRUE(discard_future_1.Wait());

  base::test::TestFuture<CrossOriginStorageStatus> append_future_2;
  writer_2.AsyncCall(&CrossOriginStorageStagingWriter::Append)
      .WithArgs(std::vector<uint8_t>(kPayload.begin(), kPayload.end()))
      .Then(append_future_2.GetCallback());
  ASSERT_EQ(append_future_2.Get(), CrossOriginStorageStatus::kSuccess);

  base::test::TestFuture<CrossOriginStorageStagingWriter::Result>
      finish_future_2;
  writer_2.AsyncCall(&CrossOriginStorageStagingWriter::Finish)
      .Then(finish_future_2.GetCallback());
  CrossOriginStorageStagingWriter::Result res_2 = finish_future_2.Take();
  ASSERT_EQ(res_2.status, CrossOriginStorageStatus::kSuccess);

  CrossOriginStorageAccessSpec restricted_spec;
  base::test::TestFuture<CrossOriginStorageStatus> commit_future_2;
  registry()->Commit(kHash, kOriginB, restricted_spec, std::move(res_2),
                     commit_future_2.GetCallback());
  EXPECT_EQ(commit_future_2.Get(), CrossOriginStorageStatus::kSuccess);

  // Aborted writer 1 (kOriginA) has no read access and staging directory is
  // completely empty.
  EXPECT_EQ(OpenForRead(kOriginA, kHash).status,
            CrossOriginStorageStatus::kNotFound);
  EXPECT_EQ(OpenForRead(kOriginB, kHash).status,
            CrossOriginStorageStatus::kSuccess);
  EXPECT_TRUE(base::IsDirectoryEmpty(browser_context_->GetPath()
                                         .AppendASCII("CrossOriginStorage")
                                         .AppendASCII("staging")));

  // Now open two more concurrent staging writers that both commit
  // simultaneously on the same hash with different origin grants.
  base::test::TestFuture<base::SequenceBound<CrossOriginStorageStagingWriter>>
      create_future_3;
  base::test::TestFuture<base::SequenceBound<CrossOriginStorageStagingWriter>>
      create_future_4;
  registry()->CreateStagingWriter(kHash.algorithm(),
                                  create_future_3.GetCallback());
  registry()->CreateStagingWriter(kHash.algorithm(),
                                  create_future_4.GetCallback());

  base::SequenceBound<CrossOriginStorageStagingWriter> writer_3 =
      create_future_3.Take();
  base::SequenceBound<CrossOriginStorageStagingWriter> writer_4 =
      create_future_4.Take();
  ASSERT_TRUE(writer_3);
  ASSERT_TRUE(writer_4);

  base::test::TestFuture<CrossOriginStorageStatus> append_future_3;
  base::test::TestFuture<CrossOriginStorageStatus> append_future_4;
  writer_3.AsyncCall(&CrossOriginStorageStagingWriter::Append)
      .WithArgs(std::vector<uint8_t>(kPayload.begin(), kPayload.end()))
      .Then(append_future_3.GetCallback());
  writer_4.AsyncCall(&CrossOriginStorageStagingWriter::Append)
      .WithArgs(std::vector<uint8_t>(kPayload.begin(), kPayload.end()))
      .Then(append_future_4.GetCallback());
  ASSERT_EQ(append_future_3.Get(), CrossOriginStorageStatus::kSuccess);
  ASSERT_EQ(append_future_4.Get(), CrossOriginStorageStatus::kSuccess);

  base::test::TestFuture<CrossOriginStorageStagingWriter::Result>
      finish_future_3;
  base::test::TestFuture<CrossOriginStorageStagingWriter::Result>
      finish_future_4;
  writer_3.AsyncCall(&CrossOriginStorageStagingWriter::Finish)
      .Then(finish_future_3.GetCallback());
  writer_4.AsyncCall(&CrossOriginStorageStagingWriter::Finish)
      .Then(finish_future_4.GetCallback());
  auto res_3 = finish_future_3.Take();
  auto res_4 = finish_future_4.Take();
  EXPECT_NE(res_3.path, res_4.path);

  auto grant_c_spec = CrossOriginStorageAccessSpec::AllowedOrigins({kOriginC});

  base::test::TestFuture<CrossOriginStorageStatus> commit_future_3;
  base::test::TestFuture<CrossOriginStorageStatus> commit_future_4;
  registry()->Commit(kHash, kOriginA, restricted_spec, std::move(res_3),
                     commit_future_3.GetCallback());
  registry()->Commit(kHash, kOriginB, grant_c_spec, std::move(res_4),
                     commit_future_4.GetCallback());

  EXPECT_EQ(commit_future_3.Get(), CrossOriginStorageStatus::kSuccess);
  EXPECT_EQ(commit_future_4.Get(), CrossOriginStorageStatus::kSuccess);
  EXPECT_EQ(OpenForRead(kOriginA, kHash).status,
            CrossOriginStorageStatus::kSuccess);
  EXPECT_EQ(OpenForRead(kOriginC, kHash).status,
            CrossOriginStorageStatus::kSuccess);
  EXPECT_TRUE(base::IsDirectoryEmpty(browser_context_->GetPath()
                                         .AppendASCII("CrossOriginStorage")
                                         .AppendASCII("staging")));
}

TEST_F(CrossOriginStorageRegistryTest,
       CancelledCreateStagingWriterCleansUpOffUIThread) {
  struct Receiver {
    void OnWriterCreated(
        base::SequenceBound<CrossOriginStorageStagingWriter> writer) {
      writer_ = std::move(writer);
    }
    base::SequenceBound<CrossOriginStorageStagingWriter> writer_;
    base::WeakPtrFactory<Receiver> weak_factory_{this};
  };

  {
    // Disallow blocking on the UI thread for the entire lifetime of the
    // creation + cancellation sequence so any UI-thread file close or deletion
    // fails immediately.
    base::ScopedDisallowBlocking disallow_blocking_on_ui;

    // 1. Cancel the callback *before* CreateStagingWriter completes on the
    // background sequence.
    {
      Receiver cancelled_receiver;
      registry()->CreateStagingWriter(
          blink::mojom::CrossOriginStorageAlgorithm::kSha256,
          base::BindOnce(&Receiver::OnWriterCreated,
                         cancelled_receiver.weak_factory_.GetWeakPtr()));
    }

    // 2. Receive a valid writer on the UI thread, then synchronously reset its
    // SequenceBound wrapper from the UI thread without finishing.
    base::test::TestFuture<base::SequenceBound<CrossOriginStorageStagingWriter>>
        create_future;
    registry()->CreateStagingWriter(
        blink::mojom::CrossOriginStorageAlgorithm::kSha256,
        create_future.GetCallback());
    base::SequenceBound<CrossOriginStorageStagingWriter> writer =
        create_future.Take();
    ASSERT_TRUE(writer);
    writer.SynchronouslyResetForTest();

    base::RunLoop run_loop;
    base::ThreadPoolInstance::Get()->FlushAsyncForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Both cancelled staging files must have been closed and unlinked off the UI
  // thread leaving no orphaned .tmp files behind.
  const base::FilePath staging_dir = browser_context_->GetPath()
                                         .AppendASCII("CrossOriginStorage")
                                         .AppendASCII("staging");
  EXPECT_TRUE(base::IsDirectoryEmpty(staging_dir));
}

TEST_F(CrossOriginStorageRegistryTest, AllowedOriginsCapacityCapped) {
  const url::Origin kStoringOrigin =
      url::Origin::Create(GURL("https://store.example.com"));
  constexpr std::string_view kPayload = "capacity test payload";
  const CrossOriginStorageHash kHash = ComputeHashForString(kPayload);

  std::vector<url::Origin> first_batch;
  for (size_t i = 0; i < cos_constants::kMaxTargetOriginsLength; ++i) {
    first_batch.push_back(url::Origin::Create(
        GURL("https://site-" + base::NumberToString(i) + ".test")));
  }

  EXPECT_EQ(
      StageAndCommit(kStoringOrigin, kHash, kPayload,
                     CrossOriginStorageAccessSpec::AllowedOrigins(first_batch)),
      CrossOriginStorageStatus::kSuccess);

  // Re-committing already-present origins succeeds.
  EXPECT_EQ(StageAndCommit(
                kStoringOrigin, kHash, kPayload,
                CrossOriginStorageAccessSpec::AllowedOrigins({first_batch[0]})),
            CrossOriginStorageStatus::kSuccess);

  // Committing new distinct origins past `kMaxTargetOriginsLength` still
  // succeeds (`kSuccess`) while dropping excess origins.
  const url::Origin kExcessOrigin1 =
      url::Origin::Create(GURL("https://excess-1.test"));
  const url::Origin kExcessOrigin2 =
      url::Origin::Create(GURL("https://excess-2.test"));
  EXPECT_EQ(StageAndCommit(kStoringOrigin, kHash, kPayload,
                           CrossOriginStorageAccessSpec::AllowedOrigins(
                               {kExcessOrigin1, kExcessOrigin2})),
            CrossOriginStorageStatus::kSuccess);
  EXPECT_EQ(OpenForRead(first_batch[99], kHash).status,
            CrossOriginStorageStatus::kSuccess);
  EXPECT_EQ(OpenForRead(kExcessOrigin1, kHash).status,
            CrossOriginStorageStatus::kNotFound);
}

}  // namespace content
