// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_CONNECTION_POOL_H_
#define PRINTING_BACKEND_CUPS_CONNECTION_POOL_H_

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/sequence_checker.h"
#include "printing/backend/cups_deleters.h"

namespace printing {

// The CupsConnectionPool provides the ability to open some connections to the
// CUPS server at an early stage and then make them available for use across
// the duration of a process's lifetime.  This enables CUPS connections to be
// available from within sandboxed processes, where calls that need to open a
// new socket to the CUPS server would fail with access denied.
class COMPONENT_EXPORT(PRINT_BACKEND) CupsConnectionPool {
 public:
  // Create the connection pool, loading it with a predetermined number of
  // initial connections.
  static void Create();

  CupsConnectionPool(const CupsConnectionPool&) = delete;
  CupsConnectionPool& operator=(const CupsConnectionPool&) = delete;

  static CupsConnectionPool* GetInstance();

  // Take a connection from the pool to be used for communicating with the CUPS
  // server.  Returns nullptr if all of the connections have already been taken.
  ScopedHttpPtr TakeConnection();

  // Add a connection to the pool of available connections.
  void AddConnection(ScopedHttpPtr connection);

  // Check if there are any connections available for use.
  bool IsConnectionAvailable() const { return !connections_.empty(); }

 private:
  CupsConnectionPool();
  ~CupsConnectionPool();

  base::queue<ScopedHttpPtr> connections_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  //  namespace printing

#endif  // PRINTING_BACKEND_CUPS_CONNECTION_POOL_H_
