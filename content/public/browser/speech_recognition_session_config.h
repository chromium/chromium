// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONFIG_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONFIG_H_

#include <stdint.h>

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/browser/speech_recognition_session_preamble.h"
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
  // Accept language header. If |language| is empty, used to get a language
  // instead.
  std::string accept_language;
  std::vector<media::mojom::SpeechRecognitionGrammar> grammars;
  url::Origin origin;
  bool filter_profanities;
  bool continuous;
  bool interim_results;
  uint32_t max_hypotheses;
  bool on_device;
  bool allow_cloud_fallback;
  std::string auth_token;
  std::string auth_scope;
  scoped_refptr<SpeechRecognitionSessionPreamble> preamble;
  SpeechRecognitionSessionContext initial_context;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory;

  base::WeakPtr<SpeechRecognitionEventListener> event_listener;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONFIG_H_
