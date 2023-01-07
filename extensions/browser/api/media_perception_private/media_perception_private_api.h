// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_PRIVATE_API_H_

#include "extensions/browser/api/media_perception_private/media_perception_api_manager.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/media_perception_private.h"

namespace extensions {

class MediaPerceptionPrivateGetStateFunction : public ExtensionFunction {
 public:
  MediaPerceptionPrivateGetStateFunction();

  MediaPerceptionPrivateGetStateFunction(
      const MediaPerceptionPrivateGetStateFunction&) = delete;
  MediaPerceptionPrivateGetStateFunction& operator=(
      const MediaPerceptionPrivateGetStateFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("mediaPerceptionPrivate.getState",
                             MEDIAPERCEPTIONPRIVATE_GETSTATE)

 private:
  ~MediaPerceptionPrivateGetStateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void GetStateCallback(extensions::api::media_perception_private::State state);
};

class MediaPerceptionPrivateSetStateFunction : public ExtensionFunction {
 public:
  MediaPerceptionPrivateSetStateFunction();

  MediaPerceptionPrivateSetStateFunction(
      const MediaPerceptionPrivateSetStateFunction&) = delete;
  MediaPerceptionPrivateSetStateFunction& operator=(
      const MediaPerceptionPrivateSetStateFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("mediaPerceptionPrivate.setState",
                             MEDIAPERCEPTIONPRIVATE_SETSTATE)

 private:
  ~MediaPerceptionPrivateSetStateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void SetStateCallback(extensions::api::media_perception_private::State state);
};

class MediaPerceptionPrivateGetDiagnosticsFunction : public ExtensionFunction {
 public:
  MediaPerceptionPrivateGetDiagnosticsFunction();

  MediaPerceptionPrivateGetDiagnosticsFunction(
      const MediaPerceptionPrivateGetDiagnosticsFunction&) = delete;
  MediaPerceptionPrivateGetDiagnosticsFunction& operator=(
      const MediaPerceptionPrivateGetDiagnosticsFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("mediaPerceptionPrivate.getDiagnostics",
                             MEDIAPERCEPTIONPRIVATE_GETDIAGNOSTICS)

 private:
  ~MediaPerceptionPrivateGetDiagnosticsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void GetDiagnosticsCallback(
      extensions::api::media_perception_private::Diagnostics diagnostics);
};

class MediaPerceptionPrivateSetAnalyticsComponentFunction
    : public ExtensionFunction {
 public:
  MediaPerceptionPrivateSetAnalyticsComponentFunction();

  MediaPerceptionPrivateSetAnalyticsComponentFunction(
      const MediaPerceptionPrivateSetAnalyticsComponentFunction&) = delete;
  MediaPerceptionPrivateSetAnalyticsComponentFunction& operator=(
      const MediaPerceptionPrivateSetAnalyticsComponentFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("mediaPerceptionPrivate.setAnalyticsComponent",
                             MEDIAPERCEPTIONPRIVATE_SETANALYTICSCOMPONENT)

 private:
  ~MediaPerceptionPrivateSetAnalyticsComponentFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnAnalyticsComponentSet(
      extensions::api::media_perception_private::ComponentState
          component_state);
};

class MediaPerceptionPrivateSetComponentProcessStateFunction
    : public ExtensionFunction {
 public:
  MediaPerceptionPrivateSetComponentProcessStateFunction();

  MediaPerceptionPrivateSetComponentProcessStateFunction(
      const MediaPerceptionPrivateSetComponentProcessStateFunction&) = delete;
  MediaPerceptionPrivateSetComponentProcessStateFunction& operator=(
      const MediaPerceptionPrivateSetComponentProcessStateFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("mediaPerceptionPrivate.setComponentProcessState",
                             MEDIAPERCEPTIONPRIVATE_SETCOMPONENTPROCESSSTATE)

 private:
  ~MediaPerceptionPrivateSetComponentProcessStateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnComponentProcessStateSet(
      extensions::api::media_perception_private::ProcessState process_state);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_PRIVATE_API_H_
