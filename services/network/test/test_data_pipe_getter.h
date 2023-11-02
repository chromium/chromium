// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_DATA_PIPE_GETTER_H_
#define SERVICES_NETWORK_TEST_TEST_DATA_PIPE_GETTER_H_

#include <memory>
#include <string>

#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"

namespace network {

// A DataPipeGetter implementation for tests. It vends data pipes and writes the
// given string to them. Read() can be called multiple times, even if the caller
// closes the data pipe before all data is written. But it assumes that there is
// only one outstanding call to Read() at a time.
class TestDataPipeGetter : public mojom::DataPipeGetter {
 public:
  TestDataPipeGetter(const std::string& string_to_write,
                     mojo::PendingReceiver<mojom::DataPipeGetter> receiver);

  TestDataPipeGetter(const TestDataPipeGetter&) = delete;
  TestDataPipeGetter& operator=(const TestDataPipeGetter&) = delete;

  ~TestDataPipeGetter() override;

  // If set to anything other than net::OK, won't bother to write the data.
  void set_start_error(int32_t start_error);

  // If set to true, will advertise the body size as being 1 byte larger than
  // |string_to_write|, so the data pipe will be closed when the caller is
  // still expecting more data.
  void set_pipe_closed_early(bool pipe_closed_early);

  // mojom::DataPipeGetter implementation:
  void Read(mojo::ScopedDataPipeProducerHandle pipe,
            ReadCallback callback) override;
  void Clone(mojo::PendingReceiver<mojom::DataPipeGetter> receiver) override;

 private:
  void MojoReadyCallback(MojoResult result,
                         const mojo::HandleSignalsState& state);
  void WriteData();

  const std::string string_to_write_;
  int32_t start_error_ = 0;  // net::OK
  bool pipe_closed_early_ = false;

  mojo::ReceiverSet<mojom::DataPipeGetter> receivers_;

  mojo::ScopedDataPipeProducerHandle pipe_;
  // Must be below |pipe_|, so it's deleted first.
  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;
  size_t write_position_ = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_DATA_PIPE_GETTER_H_
