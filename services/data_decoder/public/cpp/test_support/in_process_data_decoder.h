// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_TEST_SUPPORT_IN_PROCESS_DATA_DECODER_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_TEST_SUPPORT_IN_PROCESS_DATA_DECODER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/data_decoder/public/cpp/service_provider.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom.h"

namespace data_decoder {
namespace test {

// As long as an object of this type exists, attempts to launch the Data Decoder
// service will route to an internal, in-process instance. Only used for tests
// to avoid the complexity and dependencies of a multiprocess environment.
class InProcessDataDecoder : public ServiceProvider {
 public:
  InProcessDataDecoder();
  ~InProcessDataDecoder() override;

  DataDecoderService& service() { return service_; }

  const mojo::ReceiverSet<mojom::DataDecoderService>& receivers() const {
    return receivers_;
  }

 private:
  // ServiceProvider implementation:
  void BindDataDecoderService(
      mojo::PendingReceiver<mojom::DataDecoderService> receiver) override;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  DataDecoderService service_;
  mojo::ReceiverSet<mojom::DataDecoderService> receivers_;
  base::WeakPtrFactory<InProcessDataDecoder> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InProcessDataDecoder);
};

}  // namespace test
}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_TEST_SUPPORT_IN_PROCESS_DATA_DECODER_H_
