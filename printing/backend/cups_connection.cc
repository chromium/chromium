// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_connection.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/logging.h"
#include "build/build_config.h"
#include "printing/backend/cups_helper.h"
#include "printing/backend/cups_jobs.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "printing/backend/cups_connection_pool.h"
#endif

namespace printing {

namespace {

// The number of jobs we'll retrieve for a queue.  We expect a user to queue at
// most 10 jobs per printer.  If they queue more, they won't receive updates for
// the 11th job until one finishes.
constexpr int kProcessingJobsLimit = 10;

// The number of completed jobs that are retrieved.  We only need one update for
// a completed job to confirm its final status.  We could retrieve one but we
// retrieve the last 3 in case that many finished between queries.
constexpr int kCompletedJobsLimit = 3;

class DestinationEnumerator {
 public:
  DestinationEnumerator() {}
  DestinationEnumerator(const DestinationEnumerator&) = delete;
  DestinationEnumerator& operator=(const DestinationEnumerator&) = delete;

  static int cups_callback(void* user_data, unsigned flags, cups_dest_t* dest) {
    cups_dest_t* copied_dest;
    cupsCopyDest(dest, 0, &copied_dest);
    reinterpret_cast<DestinationEnumerator*>(user_data)->store_dest(
        copied_dest);

    //  keep going
    return 1;
  }

  void store_dest(cups_dest_t* dest) { dests_.emplace_back(dest); }

  // Returns the collected destinations.
  std::vector<ScopedDestination>& get_dests() { return dests_; }

 private:
  std::vector<ScopedDestination> dests_;
};

}  // namespace

QueueStatus::QueueStatus() = default;

QueueStatus::QueueStatus(const QueueStatus& other) = default;

QueueStatus::~QueueStatus() = default;

class CupsConnectionImpl : public CupsConnection {
 public:
  CupsConnectionImpl() = default;

  CupsConnectionImpl(const CupsConnectionImpl&) = delete;
  CupsConnectionImpl& operator=(const CupsConnectionImpl&) = delete;

  ~CupsConnectionImpl() override {
#if BUILDFLAG(IS_CHROMEOS)
    if (cups_http_) {
      // If there is a connection pool, then the connection we have came from
      // it.  We must add the connection back to the pool for possible reuse
      // rather than letting it be automatically closed, since we can never get
      // it back after closing it.
      CupsConnectionPool* connection_pool = CupsConnectionPool::GetInstance();
      if (connection_pool)
        connection_pool->AddConnection(std::move(cups_http_));
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  bool GetDests(std::vector<std::unique_ptr<CupsPrinter>>& printers) override {
    printers.clear();
    if (!Connect()) {
      LOG(WARNING) << "CUPS connection failed: ";
      return false;
    }
    DestinationEnumerator enumerator;
    const int success =
        cupsEnumDests(CUPS_DEST_FLAGS_NONE, kCupsTimeoutMs,
                      /*cancel=*/nullptr,
                      /*type=*/CUPS_PRINTER_LOCAL, kDestinationsFilterMask,
                      &DestinationEnumerator::cups_callback, &enumerator);

    if (!success) {
      LOG(WARNING) << "Enumerating printers failed";
      return false;
    }

    auto dests = std::move(enumerator.get_dests());
    for (auto& dest : dests) {
      printers.push_back(
          CupsPrinter::Create(cups_http_.get(), std::move(dest)));
    }

    return true;
  }

  std::unique_ptr<CupsPrinter> GetPrinter(const std::string& name) override {
    if (!Connect())
      return nullptr;

    cups_dest_t* dest =
        cupsGetNamedDest(cups_http_.get(), name.c_str(), nullptr);
    if (!dest)
      return nullptr;

    return CupsPrinter::Create(cups_http_.get(), ScopedDestination(dest));
  }

  bool GetJobs(const std::vector<std::string>& printer_ids,
               std::vector<QueueStatus>* queues) override {
    DCHECK(queues);
    if (!Connect()) {
      LOG(ERROR) << "Could not establish connection to CUPS";
      return false;
    }

    std::vector<QueueStatus> temp_queues;

    for (const std::string& id : printer_ids) {
      temp_queues.emplace_back();
      QueueStatus* queue_status = &temp_queues.back();

      if (!printing::GetPrinterStatus(cups_http_.get(), id,
                                      &queue_status->printer_status)) {
        LOG(WARNING) << "Could not retrieve printer status for " << id;
        return false;
      }

      if (!GetCupsJobs(cups_http_.get(), id, kCompletedJobsLimit, COMPLETED,
                       &queue_status->jobs)) {
        LOG(WARNING) << "Could not get completed jobs for " << id;
        return false;
      }

      if (!GetCupsJobs(cups_http_.get(), id, kProcessingJobsLimit, PROCESSING,
                       &queue_status->jobs)) {
        LOG(WARNING) << "Could not get in progress jobs for " << id;
        return false;
      }
    }
    queues->insert(queues->end(), temp_queues.begin(), temp_queues.end());

    return true;
  }

  bool GetPrinterStatus(const std::string& printer_id,
                        PrinterStatus* printer_status) override {
    if (!Connect()) {
      LOG(ERROR) << "Could not establish connection to CUPS";
      return false;
    }
    return printing::GetPrinterStatus(cups_http_.get(), printer_id,
                                      printer_status);
  }

  int last_error() const override { return cupsLastError(); }
  std::string last_error_message() const override {
    return cupsLastErrorString();
  }

 private:
  // lazily initialize http connection
  bool Connect() {
    if (cups_http_)
      return true;  // we're already connected

#if BUILDFLAG(IS_CHROMEOS)
    // If a connection pool has been created for this process then we must
    // allocate a connection from that, and not try to create a new one now.
    CupsConnectionPool* connection_pool = CupsConnectionPool::GetInstance();
    if (connection_pool) {
      cups_http_ = connection_pool->TakeConnection();
      if (!cups_http_)
        LOG(WARNING) << "No available connections in the CUPS connection pool";
      return !!cups_http_;
    }
#endif  // BUILDFLAG(IS_CHROMEOS)

    cups_http_ = HttpConnect2(cupsServer(), ippPort(), nullptr, AF_UNSPEC,
                              HTTP_ENCRYPT_NEVER, /*blocking=*/0,
                              kCupsTimeoutMs, nullptr);
    return !!cups_http_;
  }

  ScopedHttpPtr cups_http_;
};

std::unique_ptr<CupsConnection> CupsConnection::Create() {
  return std::make_unique<CupsConnectionImpl>();
}

}  // namespace printing
