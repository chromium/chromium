// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_connection.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/logging.h"
#include "printing/backend/cups_jobs.h"

namespace printing {

namespace {

constexpr int kTimeoutMs = 3000;

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
  CupsConnectionImpl(const GURL& print_server_url,
                     http_encryption_t encryption,
                     bool blocking)
      : print_server_url_(print_server_url),
        cups_encryption_(encryption),
        blocking_(false),
        cups_http_(nullptr) {}

  CupsConnectionImpl(CupsConnectionImpl&& connection)
      : print_server_url_(connection.print_server_url_),
        cups_encryption_(connection.cups_encryption_),
        blocking_(connection.blocking_),
        cups_http_(std::move(connection.cups_http_)) {}

  ~CupsConnectionImpl() override {}

  std::vector<std::unique_ptr<CupsPrinter>> GetDests() override {
    if (!Connect()) {
      LOG(WARNING) << "CUPS connection failed";
      return std::vector<std::unique_ptr<CupsPrinter>>();
    }

    // On macOS, AirPrint destinations show up even if they're not added to the
    // system, and their capabilities cannot be read in that situation
    // (crbug.com/1027834). Therefore, only show discovered destinations that
    // have been added locally. Also exclude fax and scanner devices.
    constexpr cups_ptype_t kMask =
        CUPS_PRINTER_FAX | CUPS_PRINTER_SCANNER | CUPS_PRINTER_DISCOVERED;
    DestinationEnumerator enumerator;
    const int success =
        cupsEnumDests(CUPS_DEST_FLAGS_NONE, kTimeoutMs,
                      /*cancel=*/nullptr,
                      /*type=*/CUPS_PRINTER_LOCAL, kMask,
                      &DestinationEnumerator::cups_callback, &enumerator);

    if (!success) {
      LOG(WARNING) << "Enumerating printers failed";
      return std::vector<std::unique_ptr<CupsPrinter>>();
    }

    auto dests = std::move(enumerator.get_dests());
    std::vector<std::unique_ptr<CupsPrinter>> printers;
    for (auto& dest : dests) {
      printers.push_back(
          CupsPrinter::Create(cups_http_.get(), std::move(dest)));
    }

    return printers;
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

  std::string server_name() const override { return print_server_url_.host(); }

  int last_error() const override { return cupsLastError(); }
  std::string last_error_message() const override {
    return cupsLastErrorString();
  }

 private:
  // lazily initialize http connection
  bool Connect() {
    if (cups_http_)
      return true;  // we're already connected

    std::string host;
    int port;

    if (!print_server_url_.is_empty()) {
      host = print_server_url_.host();
      port = print_server_url_.IntPort();
    } else {
      host = cupsServer();
      port = ippPort();
    }

    cups_http_.reset(httpConnect2(host.c_str(), port, nullptr, AF_UNSPEC,
                                  cups_encryption_, blocking_ ? 1 : 0,
                                  kTimeoutMs, nullptr));
    return !!cups_http_;
  }

  GURL print_server_url_;
  http_encryption_t cups_encryption_;
  bool blocking_;

  ScopedHttpPtr cups_http_;
};

std::unique_ptr<CupsConnection> CupsConnection::Create(
    const GURL& print_server_url,
    http_encryption_t encryption,
    bool blocking) {
  return std::make_unique<CupsConnectionImpl>(print_server_url, encryption,
                                              blocking);
}

}  // namespace printing
