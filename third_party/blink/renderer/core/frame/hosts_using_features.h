// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_HOSTS_USING_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_HOSTS_USING_FEATURES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Document;
class EventTarget;
class ScriptState;

class CORE_EXPORT HostsUsingFeatures {
  DISALLOW_NEW();

 public:
  ~HostsUsingFeatures();

  // Features for RAPPOR. Do not reorder or remove!
  enum class Feature {
    kElementCreateShadowRoot_Unused,
    kDocumentRegisterElement_Unused,
    kEventPath_Unused,
    kDeviceMotionInsecureHost_Unused,
    kDeviceOrientationInsecureHost_Unused,
    kFullscreenInsecureHost,
    kGeolocationInsecureHost,
    kGetUserMediaInsecureHost,
    kGetUserMediaSecureHost,
    kElementAttachShadow_Unused,
    kApplicationCacheManifestSelectInsecureHost,
    kApplicationCacheAPIInsecureHost,
    kRTCPeerConnectionAudio,
    kRTCPeerConnectionVideo,
    kRTCPeerConnectionDataChannel,
    kRTCPeerConnectionUsed,  // Used to compute the "unconnected PCs" feature

    kNumberOfFeatures  // This must be the last item.
  };

  static void CountAnyWorld(Document&, Feature);
  static void CountMainWorldOnly(const ScriptState*, Document&, Feature);
  static void CountHostOrIsolatedWorldHumanReadableName(const ScriptState*,
                                                        EventTarget&,
                                                        Feature);

  void DocumentDetached(Document&);
  void UpdateMeasurementsAndClear();

  class CORE_EXPORT Value {
    DISALLOW_NEW();

   public:
    Value();

    bool IsEmpty() const { return !count_bits_; }
    void Clear() { count_bits_ = 0; }

    void Count(Feature);
    bool Get(Feature feature) const {
      return count_bits_ & (1 << static_cast<unsigned>(feature));
    }

    void Aggregate(Value);
    void RecordHostToRappor(const String& host);
    void RecordETLDPlus1ToRappor(const KURL&);

   private:
    unsigned count_bits_ : static_cast<unsigned>(Feature::kNumberOfFeatures);
  };

  void Clear();

 private:
  void RecordHostToRappor();
  void RecordNamesToRappor();
  void RecordETLDPlus1ToRappor();

  Vector<std::pair<KURL, HostsUsingFeatures::Value>, 1> url_and_values_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_HOSTS_USING_FEATURES_H_
