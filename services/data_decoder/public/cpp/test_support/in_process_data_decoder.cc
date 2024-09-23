// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"

#include <utility>

#include "base/features.h"
#include "base/rust_buildflags.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace data_decoder {
namespace test {

InProcessDataDecoder::InProcessDataDecoder()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  ServiceProvider::Set(this);
}

InProcessDataDecoder::~InProcessDataDecoder() {
  ServiceProvider::Set(nullptr);
}

std::unique_ptr<data_decoder::mojom::ImageDecoder>
InProcessDataDecoder::CreateCustomImageDecoder() {
  return nullptr;
}

void InProcessDataDecoder::SimulateJsonParserCrash(bool drop) {
#if BUILDFLAG(BUILD_RUST_JSON_READER)
  CHECK(!base::FeatureList::IsEnabled(base::features::kUseRustJsonParser))
      << "Rust JSON parser is in-process and cannot crash.";
#endif  // BUILDFLAG(BUILD_RUST_JSON_READER)
  drop_json_parsers_ = drop;
}

void InProcessDataDecoder::BindDataDecoderService(
    mojo::PendingReceiver<mojom::DataDecoderService> receiver) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&InProcessDataDecoder::BindDataDecoderService,
                       weak_ptr_factory_.GetWeakPtr(), std::move(receiver)));
    return;
  }

  receivers_.Add(this, std::move(receiver));
}

mojom::DataDecoderService* InProcessDataDecoder::GetForwardingInterface() {
  return &service_;
}

void InProcessDataDecoder::BindImageDecoder(
    mojo::PendingReceiver<mojom::ImageDecoder> receiver) {
  if (drop_image_decoders_) {
    return;
  }

  std::unique_ptr<data_decoder::mojom::ImageDecoder> custom_decoder =
      CreateCustomImageDecoder();
  if (custom_decoder) {
    mojo::MakeSelfOwnedReceiver(std::move(custom_decoder), std::move(receiver));
    return;
  }

  GetForwardingInterface()->BindImageDecoder(std::move(receiver));
}

void InProcessDataDecoder::BindJsonParser(
    mojo::PendingReceiver<mojom::JsonParser> receiver) {
  if (!drop_json_parsers_) {
    GetForwardingInterface()->BindJsonParser(std::move(receiver));
  }
}

void InProcessDataDecoder::BindWebBundleParserFactory(
    mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>
        receiver) {
  if (web_bundle_parser_factory_binder_) {
    web_bundle_parser_factory_binder_.Run(std::move(receiver));
  } else {
    GetForwardingInterface()->BindWebBundleParserFactory(std::move(receiver));
  }
}

}  // namespace test
}  // namespace data_decoder
