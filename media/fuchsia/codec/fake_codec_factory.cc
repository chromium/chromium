// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/codec/fake_codec_factory.h"

#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr fuchsia::math::SizeU k480CodedSize = {
    .width = 852,
    .height = 480,
};

constexpr fuchsia::math::SizeU k4kCodedSize = {
    .width = 3840,
    .height = 2160,
};

}  // namespace

namespace media {

FakeCodecFactory::FakeCodecFactory() = default;
FakeCodecFactory::~FakeCodecFactory() = default;

void FakeCodecFactory::Bind(
    fidl::InterfaceRequest<fuchsia::mediacodec::CodecFactory> request) {
  binding_.AddBinding(this, std::move(request));
}

void FakeCodecFactory::GetDetailedCodecDescriptions(
    GetDetailedCodecDescriptionsCallback callback) {
  std::vector<fuchsia::mediacodec::DecoderProfileDescription> profile_list;
  profile_list.push_back(std::move(
      fuchsia::mediacodec::DecoderProfileDescription()
          .set_profile(fuchsia::media::CodecProfile::VP9PROFILE_PROFILE0)
          .set_max_image_size(k4kCodedSize)
          .set_min_image_size(k480CodedSize)
          .set_allow_encryption(true)
          .set_require_encryption(false)));

  std::vector<fuchsia::mediacodec::DetailedCodecDescription> descriptions;
  descriptions.push_back(
      std::move(fuchsia::mediacodec::DetailedCodecDescription()
                    .set_codec_type(fuchsia::mediacodec::CodecType::DECODER)
                    .set_mime_type("video/vp9")
                    .set_is_hw(true)
                    .set_profile_descriptions(
                        std::move(fuchsia::mediacodec::ProfileDescriptions()
                                      .set_decoder_profile_descriptions(
                                          std::move(profile_list))))));
  fuchsia::mediacodec::CodecFactoryGetDetailedCodecDescriptionsResponse
      response;
  response.set_codecs(std::move(descriptions));
  callback(std::move(response));
}

void FakeCodecFactory::NotImplemented_(const std::string& name) {
  ADD_FAILURE() << "NotImplemented_: " << name;
}

}  // namespace media
