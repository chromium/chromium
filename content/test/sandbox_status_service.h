// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_SANDBOX_STATUS_SERVICE_H_
#define CONTENT_TEST_SANDBOX_STATUS_SERVICE_H_

#include "content/test/sandbox_status.test-mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class SandboxStatusService : public mojom::SandboxStatusService {
 public:
  static void MakeSelfOwnedReceiver(
      mojo::PendingReceiver<mojom::SandboxStatusService> receiver);

  SandboxStatusService();
  SandboxStatusService(const SandboxStatusService&) = delete;
  SandboxStatusService& operator=(const SandboxStatusService&) = delete;
  ~SandboxStatusService() override;

 private:
  // mojom::SandboxStatusService:
  void GetSandboxStatus(GetSandboxStatusCallback callback) override;
};

}  // namespace content

#endif  // CONTENT_TEST_SANDBOX_STATUS_SERVICE_H_
