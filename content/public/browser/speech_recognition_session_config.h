// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONFIG_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONFIG_H_

#include <stdint.h>

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/browser/speech_recognition_session_preamble.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_grammar.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/origin.h"

namespace content {

class SpeechRecognitionEventListener;

// Configuration params for creating a new speech recognition session.
struct CONTENT_EXPORT SpeechRecognitionSessionConfig {
  SpeechRecognitionSessionConfig();
  SpeechRecognitionSessionConfig(const SpeechRecognitionSessionConfig& other);
  ~SpeechRecognitionSessionConfig();

  std::string language;
  std::vector<media::mojom::SpeechRecognitionGrammar> grammars;
  std::optional<media::SpeechRecognitionRecognitionContext> recognition_context;
  url::Origin origin;
  bool filter_profanities = false;
  bool continuous = false;
  bool interim_results = false;
  uint32_t max_hypotheses = 1;
  bool on_device = false;  // Whether on-device speech recognition must be used.
  bool on_device_available = false;  // Whether on-device speech recognition is
                                     // installed and available.
  bool allow_cloud_fallback = false;
  std::string auth_token;
  std::string auth_scope;
  scoped_refptr<SpeechRecognitionSessionPreamble> preamble;
  SpeechRecognitionSessionContext initial_context;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory;

  base::WeakPtr<SpeechRecognitionEventListener> event_listener;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONFIG_H_
