// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/test/content_browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"

namespace content {

using DataDecoderBrowserTest = ContentBrowserTest;

class ServiceProcessObserver : public ServiceProcessHost::Observer {
 public:
  ServiceProcessObserver() { ServiceProcessHost::AddObserver(this); }

  ~ServiceProcessObserver() override {
    ServiceProcessHost::RemoveObserver(this);
  }

  int instances_started() const { return instances_started_; }

  void WaitForNextLaunch() {
    launch_wait_loop_.emplace();
    launch_wait_loop_->Run();
  }

  void OnServiceProcessLaunched(const ServiceProcessInfo& info) override {
    if (info.IsService<data_decoder::mojom::DataDecoderService>()) {
      ++instances_started_;
      if (launch_wait_loop_)
        launch_wait_loop_->Quit();
    }
  }

 private:
  base::Optional<base::RunLoop> launch_wait_loop_;
  int instances_started_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ServiceProcessObserver);
};

IN_PROC_BROWSER_TEST_F(DataDecoderBrowserTest, Launch) {
  ServiceProcessObserver observer;

  // Verifies that the DataDecoder client object launches a service process as
  // needed.
  data_decoder::DataDecoder decoder;

  // |GetService()| must always ensure a connection to the service on all
  // platforms, so we use it instead of a more specific API whose behavior may
  // vary across platforms.
  decoder.GetService();

  observer.WaitForNextLaunch();
  EXPECT_EQ(1, observer.instances_started());
}

IN_PROC_BROWSER_TEST_F(DataDecoderBrowserTest, LaunchIsolated) {
  ServiceProcessObserver observer;

  // Verifies that separate DataDecoder client objects will launch separate
  // service processes. We also bind a JsonParser interface to ensure that the
  // instances don't go idle.
  data_decoder::DataDecoder decoder1;
  mojo::Remote<data_decoder::mojom::JsonParser> parser1;
  decoder1.GetService()->BindJsonParser(parser1.BindNewPipeAndPassReceiver());
  observer.WaitForNextLaunch();
  EXPECT_EQ(1, observer.instances_started());

  data_decoder::DataDecoder decoder2;
  mojo::Remote<data_decoder::mojom::JsonParser> parser2;
  decoder2.GetService()->BindJsonParser(parser2.BindNewPipeAndPassReceiver());
  observer.WaitForNextLaunch();
  EXPECT_EQ(2, observer.instances_started());

  // Both interfaces should be connected end-to-end.
  parser1.FlushForTesting();
  parser2.FlushForTesting();
  EXPECT_TRUE(parser1.is_connected());
  EXPECT_TRUE(parser2.is_connected());
}

}  // namespace content
