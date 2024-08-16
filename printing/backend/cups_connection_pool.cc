// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_connection_pool.h"

#include <cups/cups.h>

#include "base/check.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "printing/backend/cups_connection.h"
#include "printing/backend/cups_deleters.h"
#include "printing/backend/cups_helper.h"

namespace printing {

namespace {

// There needs to be a separate connection for each thread which will connect to
// the CUPS server.  While the CUPS library is thread safe, a connection cannot
// be shared between threads.
// Issuing multiple print jobs concurrently to a single destination can be
// be queued up for submission across the same connection (they would be
// serialized at the device anyway).  There is benefit to supporting concurrent
// printing to different destinations.  It is unlikely that this would be done
// to more than a handful of destinations at once.  Pick a somewhat arbitrary
// low number for the number of allowed connections, knowing that one of these
// connections will be needed by PrintBackend.  If a user were to swamp printing
// with jobs to more unique destinations at the same time then the PrintBackend
// service could be made to throttle those and queue them up awaiting a
// connection to use.
constexpr int kNumCupsConnections = 5;

CupsConnectionPool* g_cups_connection_pool_singleton = nullptr;

}  // namespace

CupsConnectionPool::CupsConnectionPool() = default;

CupsConnectionPool::~CupsConnectionPool() = default;

// static
void CupsConnectionPool::Create() {
  DCHECK(!g_cups_connection_pool_singleton);

  // The pool is only used for connections to default server over the default
  // IPP port.  These connections are never closed; they are reused until the
  // the process terminates.
  const int port = ippPort();
  const char* server = cupsServer();
  g_cups_connection_pool_singleton = new CupsConnectionPool();
  VLOG(1) << "Creating CUPS connection pool seeded with " << kNumCupsConnections
          << " connections";
  for (auto i = 0; i < kNumCupsConnections; ++i) {
    ScopedHttpPtr connection = HttpConnect2(
        server, port, /*addrlist=*/nullptr, AF_UNSPEC, HTTP_ENCRYPTION_NEVER,
        /*blocking*/ 0, kCupsTimeoutMs, /*cancel=*/nullptr);
    if (!connection) {
      LOG(ERROR) << "Unable to create CUPS connection: "
                 << cupsLastErrorString();
      break;
    }
    g_cups_connection_pool_singleton->AddConnection(std::move(connection));
  }
}

// static
CupsConnectionPool* CupsConnectionPool::GetInstance() {
  return g_cups_connection_pool_singleton;
}

ScopedHttpPtr CupsConnectionPool::TakeConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (connections_.empty())
    return nullptr;

  ScopedHttpPtr connection = std::move(connections_.front());
  connections_.pop();
  return connection;
}

void CupsConnectionPool::AddConnection(ScopedHttpPtr connection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(connection);
  connections_.emplace(std::move(connection));
}

}  //  namespace printing
