// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

#include <string>

#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EchoCancellationMode {
  kDisabled = 0,
  kBrowserDecides = 1,
  kRemoteOnly = 2,
  kAll = 3,
  kMaxValue = kAll,
};

extern PLATFORM_EXPORT const char kEchoCancellationModeAll[];
extern PLATFORM_EXPORT const char kEchoCancellationModeRemoteOnly[];

PLATFORM_EXPORT const char* EchoCancellationModeToString(EchoCancellationMode);

// The result of parsing media stream constraints.
struct PLATFORM_EXPORT AudioProcessingProperties {
  enum class VoiceIsolationType {
    // Voice isolation behavior selected by the system is used.
    kVoiceIsolationDefault,
    // Voice isolation is disabled.
    kVoiceIsolationDisabled,
    // Voice isolation is enabled.
    kVoiceIsolationEnabled,
  };

  // Disables properties that are enabled by default.
  static const AudioProcessingProperties& Disabled();

  bool HasSameReconfigurableSettings(
      const AudioProcessingProperties& other) const;

  bool HasSameNonReconfigurableSettings(
      const AudioProcessingProperties& other) const;

  std::string ToString() const;

  EchoCancellationMode echo_cancellation_mode =
      EchoCancellationMode::kBrowserDecides;
  bool auto_gain_control = true;
  bool noise_suppression = true;
  VoiceIsolationType voice_isolation =
      VoiceIsolationType::kVoiceIsolationDefault;
};

// Which echo canceller to run and where - based on AudioProcessingProperties.
class PLATFORM_EXPORT EchoCanceller {
 public:
  enum class Type {
    kNone,
    kPlatformProvided,
    kChromeWide,
    kLoopbackBased,
    kPeerConnection
  };

  enum class ApmLocation { kRenderer, kAudioService };

  static bool IsSystemWideAecAvailable(int available_platform_effects);

  static bool IsPlatformAecAvailable(int available_platform_effects);

  static EchoCanceller From(const AudioProcessingProperties& properties,
                            int available_platform_effects);

  static EchoCanceller From(EchoCancellationMode mode,
                            int available_platform_effects);

  static EchoCanceller MakeForTesting(EchoCanceller::Type type);

  Type type() const { return type_; }

  bool IsEnabled() const { return type_ != Type::kNone; }

  bool IsPlatformProvided() const { return type_ == Type::kPlatformProvided; }

  bool IsChromeProvided() const {
    return type_ == Type::kChromeWide || type_ == Type::kLoopbackBased ||
           type_ == Type::kPeerConnection;
  }

  bool NeedSystemLoopback() const { return type_ == Type::kLoopbackBased; }

  ApmLocation GetApmLocation() const;

  const char* ToString() const;

 private:
  friend class MediaStreamAudioProcessingLayout;

  explicit EchoCanceller(Type type) : type_(type) {}

  static Type GetPreferredAec(int available_platform_effects);
  static Type GetSystemWideAec(int available_platform_effects);

  const Type type_ = Type::kNone;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
