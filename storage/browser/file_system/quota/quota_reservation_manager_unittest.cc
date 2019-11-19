// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/file_system/quota/open_file_handle.h"
#include "storage/browser/file_system/quota/quota_reservation.h"
#include "storage/browser/file_system/quota/quota_reservation_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using storage::kFileSystemTypeTemporary;
using storage::OpenFileHandle;
using storage::QuotaReservation;
using storage::QuotaReservationManager;

namespace content {

namespace {

const url::Origin kOrigin(url::Origin::Create(GURL("http://example.com")));
const storage::FileSystemType kType = kFileSystemTypeTemporary;
const int64_t kInitialFileSize = 1;

using ReserveQuotaCallback = QuotaReservationManager::ReserveQuotaCallback;

int64_t GetFileSize(const base::FilePath& path) {
  int64_t size = 0;
  base::GetFileSize(path, &size);
  return size;
}

void SetFileSize(const base::FilePath& path, int64_t size) {
  base::File file(path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  ASSERT_TRUE(file.SetLength(size));
}

class FakeBackend : public QuotaReservationManager::QuotaBackend {
 public:
  FakeBackend()
      : on_memory_usage_(kInitialFileSize), on_disk_usage_(kInitialFileSize) {}
  ~FakeBackend() override = default;

  void ReserveQuota(const url::Origin& origin,
                    storage::FileSystemType type,
                    int64_t delta,
                    ReserveQuotaCallback callback) override {
    EXPECT_EQ(kOrigin, origin);
    EXPECT_EQ(kType, type);
    on_memory_usage_ += delta;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(base::IgnoreResult(std::move(callback)),
                                  base::File::FILE_OK, delta));
  }

  void ReleaseReservedQuota(const url::Origin& origin,
                            storage::FileSystemType type,
                            int64_t size) override {
    EXPECT_LE(0, size);
    EXPECT_EQ(kOrigin, origin);
    EXPECT_EQ(kType, type);
    on_memory_usage_ -= size;
  }

  void CommitQuotaUsage(const url::Origin& origin,
                        storage::FileSystemType type,
                        int64_t delta) override {
    EXPECT_EQ(kOrigin, origin);
    EXPECT_EQ(kType, type);
    on_disk_usage_ += delta;
    on_memory_usage_ += delta;
  }

  void IncrementDirtyCount(const url::Origin& origin,
                           storage::FileSystemType type) override {}
  void DecrementDirtyCount(const url::Origin& origin,
                           storage::FileSystemType type) override {}

  int64_t on_memory_usage() { return on_memory_usage_; }
  int64_t on_disk_usage() { return on_disk_usage_; }

 private:
  int64_t on_memory_usage_;
  int64_t on_disk_usage_;

  DISALLOW_COPY_AND_ASSIGN(FakeBackend);
};

class FakeWriter {
 public:
  explicit FakeWriter(std::unique_ptr<OpenFileHandle> handle)
      : handle_(std::move(handle)),
        path_(handle_->platform_path()),
        max_written_offset_(handle_->GetEstimatedFileSize()),
        append_mode_write_amount_(0),
        dirty_(false) {}

  ~FakeWriter() {
    if (handle_)
      EXPECT_FALSE(dirty_);
  }

  int64_t Truncate(int64_t length) {
    int64_t consumed = 0;

    if (max_written_offset_ < length) {
      consumed = length - max_written_offset_;
      max_written_offset_ = length;
    }
    SetFileSize(path_, length);
    return consumed;
  }

  int64_t Write(int64_t max_offset) {
    dirty_ = true;

    int64_t consumed = 0;
    if (max_written_offset_ < max_offset) {
      consumed = max_offset - max_written_offset_;
      max_written_offset_ = max_offset;
    }
    if (GetFileSize(path_) < max_offset)
      SetFileSize(path_, max_offset);
    return consumed;
  }

  int64_t Append(int64_t amount) {
    dirty_ = true;
    append_mode_write_amount_ += amount;
    SetFileSize(path_, GetFileSize(path_) + amount);
    return amount;
  }

  void ReportUsage() {
    handle_->UpdateMaxWrittenOffset(max_written_offset_);
    handle_->AddAppendModeWriteAmount(append_mode_write_amount_);
    max_written_offset_ = handle_->GetEstimatedFileSize();
    append_mode_write_amount_ = 0;
    dirty_ = false;
  }

  void ClearWithoutUsageReport() { handle_.reset(); }

 private:
  std::unique_ptr<OpenFileHandle> handle_;
  base::FilePath path_;
  int64_t max_written_offset_;
  int64_t append_mode_write_amount_;
  bool dirty_;
};

void ExpectSuccess(bool* done, base::File::Error error) {
  EXPECT_FALSE(*done);
  *done = true;
  EXPECT_EQ(base::File::FILE_OK, error);
}

void RefreshReservation(QuotaReservation* reservation, int64_t size) {
  DCHECK(reservation);

  bool done = false;
  reservation->RefreshReservation(size, base::BindOnce(&ExpectSuccess, &done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(done);
}

}  // namespace

class QuotaReservationManagerTest : public testing::Test {
 public:
  QuotaReservationManagerTest() = default;
  ~QuotaReservationManagerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(work_dir_.CreateUniqueTempDir());
    file_path_ = work_dir_.GetPath().Append(FILE_PATH_LITERAL("hoge"));
    SetFileSize(file_path_, kInitialFileSize);

    std::unique_ptr<QuotaReservationManager::QuotaBackend> backend(
        new FakeBackend);
    reservation_manager_.reset(new QuotaReservationManager(std::move(backend)));
  }

  void TearDown() override { reservation_manager_.reset(); }

  FakeBackend* fake_backend() {
    return static_cast<FakeBackend*>(reservation_manager_->backend_.get());
  }

  QuotaReservationManager* reservation_manager() {
    return reservation_manager_.get();
  }

  const base::FilePath& file_path() const { return file_path_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir work_dir_;
  base::FilePath file_path_;
  std::unique_ptr<QuotaReservationManager> reservation_manager_;

  DISALLOW_COPY_AND_ASSIGN(QuotaReservationManagerTest);
};

TEST_F(QuotaReservationManagerTest, BasicTest) {
  scoped_refptr<QuotaReservation> reservation =
      reservation_manager()->CreateReservation(kOrigin, kType);

  {
    RefreshReservation(reservation.get(), 10 + 20 + 3);
    int64_t cached_reserved_quota = reservation->remaining_quota();
    FakeWriter writer(reservation->GetOpenFileHandle(file_path()));

    cached_reserved_quota -= writer.Write(kInitialFileSize + 10);
    EXPECT_LE(0, cached_reserved_quota);
    cached_reserved_quota -= writer.Append(20);
    EXPECT_LE(0, cached_reserved_quota);

    writer.ReportUsage();
  }

  EXPECT_EQ(3, reservation->remaining_quota());
  EXPECT_EQ(kInitialFileSize + 10 + 20, GetFileSize(file_path()));
  EXPECT_EQ(kInitialFileSize + 10 + 20, fake_backend()->on_disk_usage());
  EXPECT_EQ(kInitialFileSize + 10 + 20 + 3, fake_backend()->on_memory_usage());

  {
    RefreshReservation(reservation.get(), 5);
    FakeWriter writer(reservation->GetOpenFileHandle(file_path()));

    EXPECT_EQ(0, writer.Truncate(3));

    writer.ReportUsage();
  }

  EXPECT_EQ(5, reservation->remaining_quota());
  EXPECT_EQ(3, GetFileSize(file_path()));
  EXPECT_EQ(3, fake_backend()->on_disk_usage());
  EXPECT_EQ(3 + 5, fake_backend()->on_memory_usage());

  reservation = nullptr;

  EXPECT_EQ(3, fake_backend()->on_memory_usage());
}

TEST_F(QuotaReservationManagerTest, MultipleWriter) {
  scoped_refptr<QuotaReservation> reservation =
      reservation_manager()->CreateReservation(kOrigin, kType);

  {
    RefreshReservation(reservation.get(), 10 + 20 + 30 + 40 + 5);
    int64_t cached_reserved_quota = reservation->remaining_quota();
    FakeWriter writer1(reservation->GetOpenFileHandle(file_path()));
    FakeWriter writer2(reservation->GetOpenFileHandle(file_path()));
    FakeWriter writer3(reservation->GetOpenFileHandle(file_path()));

    cached_reserved_quota -= writer1.Write(kInitialFileSize + 10);
    EXPECT_LE(0, cached_reserved_quota);
    cached_reserved_quota -= writer2.Write(kInitialFileSize + 20);
    cached_reserved_quota -= writer3.Append(30);
    EXPECT_LE(0, cached_reserved_quota);
    cached_reserved_quota -= writer3.Append(40);
    EXPECT_LE(0, cached_reserved_quota);

    writer1.ReportUsage();
    writer2.ReportUsage();
    writer3.ReportUsage();
  }

  EXPECT_EQ(kInitialFileSize + 20 + 30 + 40, GetFileSize(file_path()));
  EXPECT_EQ(kInitialFileSize + 10 + 20 + 30 + 40 + 5,
            fake_backend()->on_memory_usage());
  EXPECT_EQ(kInitialFileSize + 20 + 30 + 40, fake_backend()->on_disk_usage());

  reservation = nullptr;

  EXPECT_EQ(kInitialFileSize + 20 + 30 + 40, fake_backend()->on_disk_usage());
}

TEST_F(QuotaReservationManagerTest, MultipleClient) {
  scoped_refptr<QuotaReservation> reservation1 =
      reservation_manager()->CreateReservation(kOrigin, kType);
  RefreshReservation(reservation1.get(), 10);
  int64_t cached_reserved_quota1 = reservation1->remaining_quota();

  scoped_refptr<QuotaReservation> reservation2 =
      reservation_manager()->CreateReservation(kOrigin, kType);
  RefreshReservation(reservation2.get(), 20);
  int64_t cached_reserved_quota2 = reservation2->remaining_quota();

  std::unique_ptr<FakeWriter> writer1(
      new FakeWriter(reservation1->GetOpenFileHandle(file_path())));

  std::unique_ptr<FakeWriter> writer2(
      new FakeWriter(reservation2->GetOpenFileHandle(file_path())));

  cached_reserved_quota1 -= writer1->Write(kInitialFileSize + 10);
  EXPECT_LE(0, cached_reserved_quota1);

  cached_reserved_quota2 -= writer2->Append(20);
  EXPECT_LE(0, cached_reserved_quota2);

  writer1->ReportUsage();
  RefreshReservation(reservation1.get(), 2);
  cached_reserved_quota1 = reservation1->remaining_quota();

  writer2->ReportUsage();
  RefreshReservation(reservation2.get(), 3);
  cached_reserved_quota2 = reservation2->remaining_quota();

  writer1.reset();
  writer2.reset();

  EXPECT_EQ(kInitialFileSize + 10 + 20, GetFileSize(file_path()));
  EXPECT_EQ(kInitialFileSize + 10 + 20 + 2 + 3,
            fake_backend()->on_memory_usage());
  EXPECT_EQ(kInitialFileSize + 10 + 20, fake_backend()->on_disk_usage());

  reservation1 = nullptr;
  EXPECT_EQ(kInitialFileSize + 10 + 20 + 3, fake_backend()->on_memory_usage());

  reservation2 = nullptr;
  EXPECT_EQ(kInitialFileSize + 10 + 20, fake_backend()->on_memory_usage());
}

TEST_F(QuotaReservationManagerTest, ClientCrash) {
  scoped_refptr<QuotaReservation> reservation1 =
      reservation_manager()->CreateReservation(kOrigin, kType);
  RefreshReservation(reservation1.get(), 15);

  scoped_refptr<QuotaReservation> reservation2 =
      reservation_manager()->CreateReservation(kOrigin, kType);
  RefreshReservation(reservation2.get(), 20);

  {
    FakeWriter writer(reservation1->GetOpenFileHandle(file_path()));

    writer.Write(kInitialFileSize + 10);

    reservation1->OnClientCrash();
    writer.ClearWithoutUsageReport();
  }
  reservation1 = nullptr;

  EXPECT_EQ(kInitialFileSize + 10, GetFileSize(file_path()));
  EXPECT_EQ(kInitialFileSize + 15 + 20, fake_backend()->on_memory_usage());
  EXPECT_EQ(kInitialFileSize + 10, fake_backend()->on_disk_usage());

  reservation2 = nullptr;
  EXPECT_EQ(kInitialFileSize + 10, fake_backend()->on_memory_usage());
}

}  // namespace content
