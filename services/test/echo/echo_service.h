// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ECHO_ECHO_SERVICE_H_
#define SERVICES_ECHO_ECHO_SERVICE_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/test/echo/public/mojom/echo.mojom.h"

namespace echo {

class EchoService : public mojom::EchoService {
 public:
  explicit EchoService(mojo::PendingReceiver<mojom::EchoService> receiver);
  ~EchoService() override;

 private:
  // mojom::EchoService:
  void EchoString(const std::string& input,
                  EchoStringCallback callback) override;
  void Quit() override;
  void Crash() override;

  mojo::Receiver<mojom::EchoService> receiver_;

  DISALLOW_COPY_AND_ASSIGN(EchoService);
};

}  // namespace echo

#endif  // SERVICES_ECHO_ECHO_SERVICE_H_
