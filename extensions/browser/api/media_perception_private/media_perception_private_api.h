// Copyright 2017 The Chromium Authors. All rights reserved.
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
  DECLARE_EXTENSION_FUNCTION("mediaPerceptionPrivate.getState",
                             MEDIAPERCEPTIONPRIVATE_GETSTATE)

 private:
  ~MediaPerceptionPrivateGetStateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void GetStateCallback(extensions::api::media_perception_private::State state);

  DISALLOW_COPY_AND_ASSIGN(MediaPerceptionPrivateGetStateFunction);
};

class MediaPerceptionPrivateSetStateFunction : public ExtensionFunction {
 public:
  MediaPerceptionPrivateSetStateFunction();
  DECLARE_EXTENSION_FUNCTION("mediaPerceptionPrivate.setState",
                             MEDIAPERCEPTIONPRIVATE_SETSTATE)

 private:
  ~MediaPerceptionPrivateSetStateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void SetStateCallback(extensions::api::media_perception_private::State state);

  DISALLOW_COPY_AND_ASSIGN(MediaPerceptionPrivateSetStateFunction);
};

class MediaPerceptionPrivateGetDiagnosticsFunction : public ExtensionFunction {
 public:
  MediaPerceptionPrivateGetDiagnosticsFunction();
  DECLARE_EXTENSION_FUNCTION("mediaPerceptionPrivate.getDiagnostics",
                             MEDIAPERCEPTIONPRIVATE_GETDIAGNOSTICS)

 private:
  ~MediaPerceptionPrivateGetDiagnosticsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void GetDiagnosticsCallback(
      extensions::api::media_perception_private::Diagnostics diagnostics);

  DISALLOW_COPY_AND_ASSIGN(MediaPerceptionPrivateGetDiagnosticsFunction);
};

class MediaPerceptionPrivateSetAnalyticsComponentFunction
    : public ExtensionFunction {
 public:
  MediaPerceptionPrivateSetAnalyticsComponentFunction();
  DECLARE_EXTENSION_FUNCTION("mediaPerceptionPrivate.setAnalyticsComponent",
                             MEDIAPERCEPTIONPRIVATE_SETANALYTICSCOMPONENT)

 private:
  ~MediaPerceptionPrivateSetAnalyticsComponentFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnAnalyticsComponentSet(
      extensions::api::media_perception_private::ComponentState
          component_state);

  DISALLOW_COPY_AND_ASSIGN(MediaPerceptionPrivateSetAnalyticsComponentFunction);
};

class MediaPerceptionPrivateSetComponentProcessStateFunction
    : public ExtensionFunction {
 public:
  MediaPerceptionPrivateSetComponentProcessStateFunction();
  DECLARE_EXTENSION_FUNCTION("mediaPerceptionPrivate.setComponentProcessState",
                             MEDIAPERCEPTIONPRIVATE_SETCOMPONENTPROCESSSTATE)

 private:
  ~MediaPerceptionPrivateSetComponentProcessStateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnComponentProcessStateSet(
      extensions::api::media_perception_private::ProcessState process_state);

  DISALLOW_COPY_AND_ASSIGN(
      MediaPerceptionPrivateSetComponentProcessStateFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_PRIVATE_API_H_
