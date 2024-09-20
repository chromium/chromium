// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/parser/background_html_scanner.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/resource_preloader.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder_for_fuzzing.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/fuzzed_data_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

std::unique_ptr<CachedDocumentParameters> CachedDocumentParametersForFuzzing(
    FuzzedDataProvider& fuzzed_data) {
  std::unique_ptr<CachedDocumentParameters> document_parameters =
      std::make_unique<CachedDocumentParameters>();
  document_parameters->do_html_preload_scanning = fuzzed_data.ConsumeBool();
  // TODO(csharrison): How should this be fuzzed?
  document_parameters->default_viewport_min_width = Length();
  document_parameters->viewport_meta_zero_values_quirk =
      fuzzed_data.ConsumeBool();
  document_parameters->viewport_meta_enabled = fuzzed_data.ConsumeBool();
  document_parameters->integrity_features =
      fuzzed_data.ConsumeBool()
          ? SubresourceIntegrity::IntegrityFeatures::kDefault
          : SubresourceIntegrity::IntegrityFeatures::kSignatures;
  return document_parameters;
}

class MockResourcePreloader : public ResourcePreloader {
  void Preload(std::unique_ptr<PreloadRequest>) override {}
};

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;
  FuzzedDataProvider fuzzed_data(data, size);

  HTMLParserOptions options;
  options.scripting_flag = fuzzed_data.ConsumeBool();

  std::unique_ptr<CachedDocumentParameters> document_parameters =
      CachedDocumentParametersForFuzzing(fuzzed_data);

  KURL document_url("http://whatever.test/");

  // Copied from HTMLPreloadScannerTest. May be worthwhile to fuzz.
  auto media_data =
      std::make_unique<MediaValuesCached::MediaValuesCachedData>();
  media_data->viewport_width = 500;
  media_data->viewport_height = 600;
  media_data->device_width = 700;
  media_data->device_height = 800;
  media_data->device_pixel_ratio = 2.0;
  media_data->color_bits_per_component = 24;
  media_data->monochrome_bits_per_component = 0;
  media_data->primary_pointer_type =
      mojom::blink::PointerType::kPointerFineType;
  media_data->three_d_enabled = true;
  media_data->media_type = media_type_names::kScreen;
  media_data->strict_mode = true;
  media_data->display_mode = blink::mojom::DisplayMode::kBrowser;

  MockResourcePreloader preloader;

  std::unique_ptr<HTMLPreloadScanner> scanner =
      std::make_unique<HTMLPreloadScanner>(
          std::make_unique<HTMLTokenizer>(options), document_url,
          std::move(document_parameters), std::move(media_data),
          TokenPreloadScanner::ScannerType::kMainDocument, nullptr);

  TextResourceDecoderForFuzzing decoder(fuzzed_data);
  std::string bytes = fuzzed_data.ConsumeRemainingBytes();
  String decoded_bytes = decoder.Decode(bytes);
  scanner->AppendToEnd(decoded_bytes);
  std::unique_ptr<PendingPreloadData> preload_data =
      scanner->Scan(document_url);
  preloader.TakeAndPreload(preload_data->requests);
  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}
