// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_PROTO_WRAPPER_PASSKEYS_H_
#define MOJO_PUBLIC_CPP_BASE_PROTO_WRAPPER_PASSKEYS_H_

#include "base/gtest_prod_util.h"
#include "base/types/pass_key.h"

namespace component_updater {
class ReadMaskedDomainListProto;
class PKIMetadataComponentInstallerService;
}  // namespace component_updater

namespace paint_preview {
FORWARD_DECLARE_TEST(PaintPreviewCompositorBeginCompositeTest, InvalidProto);
}  // namespace paint_preview

namespace mojo_base {

// PassKey that allows people to directly name or access the bytes of a wrapped
// protobuf stream in ProtoWrapper. This PassKey can be granted to classes that
// are fetching protobuf streams from the network and want to get them into the
// mojo type system to send over mojo IPC.
//
// If the protobuf byte stream will not be sent over mojo but instead will be
// immediately deserialized then it is not necessary to use ProtoWrapper at all.
class ProtoWrapperBytes {
 public:
  using PassKey = base::PassKey<ProtoWrapperBytes>;

 private:
  static PassKey GetPassKey() { return PassKey(); }

  // Add friend classes that need direct access to the underlying span_bytes()
  // or to directly set the contained class name and bytes with from_span().
  friend class component_updater::ReadMaskedDomainListProto;
  friend class component_updater::PKIMetadataComponentInstallerService;

  // Tests.
  FRIEND_TEST_ALL_PREFIXES(ProtoWrapperTest, ToFromBytes);
  FRIEND_TEST_ALL_PREFIXES(
      paint_preview::PaintPreviewCompositorBeginCompositeTest,
      InvalidProto);
};

}  // namespace mojo_base

#endif  // MOJO_PUBLIC_CPP_BASE_PROTO_WRAPPER_PASSKEYS_H_
