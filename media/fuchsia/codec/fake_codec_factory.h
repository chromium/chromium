// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CODEC_FAKE_CODEC_FACTORY_H_
#define MEDIA_FUCHSIA_CODEC_FAKE_CODEC_FACTORY_H_

#include <fuchsia/media/cpp/fidl_test_base.h>
#include <fuchsia/mediacodec/cpp/fidl.h>
#include <fuchsia/mediacodec/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>

#include "base/fuchsia/scoped_service_binding.h"

namespace media {

// Only `GetDetailedCodecDescriptions` is implemented with a fake VP9 video
// codec description.
class FakeCodecFactory
    : public fuchsia::mediacodec::testing::CodecFactory_TestBase {
 public:
  explicit FakeCodecFactory();
  FakeCodecFactory(const FakeCodecFactory&) = delete;
  FakeCodecFactory& operator=(const FakeCodecFactory&) = delete;
  ~FakeCodecFactory() override;

  void Bind(fidl::InterfaceRequest<fuchsia::mediacodec::CodecFactory> request);

  // Implementation of fuchsia::mediacodec::testing::CodecFactory_TestBase.
  void GetDetailedCodecDescriptions(
      GetDetailedCodecDescriptionsCallback callback) override;
  void NotImplemented_(const std::string& name) override;

 private:
  fidl::BindingSet<fuchsia::mediacodec::CodecFactory> binding_;
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_CODEC_FAKE_CODEC_FACTORY_H_
