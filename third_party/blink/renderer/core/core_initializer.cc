/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/core_initializer.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/css/media_feature_names.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/event_factory.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/html_tokenizer_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/timezone/timezone_controller.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/core/xmlns_names.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_table.h"

namespace blink {

CoreInitializer* CoreInitializer::instance_ = nullptr;

void CoreInitializer::RegisterEventFactory() {
  static bool is_registered = false;
  if (is_registered)
    return;
  is_registered = true;

  Document::RegisterEventFactory(EventFactory::Create());
}

void CoreInitializer::Initialize() {
  // Initialize must be called once by singleton ModulesInitializer.
  DCHECK(!instance_);
  instance_ = this;
  // Note: in order to add core static strings for a new module (1)
  // the value of 'coreStaticStringsCount' must be updated with the
  // added strings count, (2) if the added strings are quialified names
  // the 'qualifiedNamesCount' must be updated as well, (3) the strings
  // 'init()' function call must be added.
  // TODO(mikhail.pozdnyakov@intel.com): We should generate static strings
  // initialization code.
  const unsigned kQualifiedNamesCount =
      html_names::kTagsCount + html_names::kAttrsCount +
      mathml_names::kTagsCount + mathml_names::kAttrsCount +
      svg_names::kTagsCount + svg_names::kAttrsCount +
      xlink_names::kAttrsCount + xml_names::kAttrsCount +
      xmlns_names::kAttrsCount;

  const unsigned kCoreStaticStringsCount =
      kQualifiedNamesCount + event_interface_names::kNamesCount +
      event_target_names::kNamesCount + event_type_names::kNamesCount +
      fetch_initiator_type_names::kNamesCount + font_family_names::kNamesCount +
      html_tokenizer_names::kNamesCount + http_names::kNamesCount +
      input_type_names::kNamesCount + keywords::kNamesCount +
      media_feature_names::kNamesCount + media_type_names::kNamesCount +
      performance_entry_names::kNamesCount;

  StringImpl::ReserveStaticStringsCapacityForSize(
      kCoreStaticStringsCount + StringImpl::AllStaticStrings().size());
  QualifiedName::InitAndReserveCapacityForSize(kQualifiedNamesCount);

  AtomicStringTable::Instance().ReserveCapacity(kCoreStaticStringsCount);

  html_names::Init();
  mathml_names::Init();
  svg_names::Init();
  xlink_names::Init();
  xml_names::Init();
  xmlns_names::Init();

  event_interface_names::Init();
  event_target_names::Init();
  event_type_names::Init();
  fetch_initiator_type_names::Init();
  font_family_names::Init();
  html_tokenizer_names::Init();
  http_names::Init();
  input_type_names::Init();
  keywords::Init();
  media_feature_names::Init();
  media_type_names::Init();
  performance_entry_names::Init();

  MediaQueryEvaluator::Init();
  CSSParserTokenRange::InitStaticEOFToken();

  style_change_extra_data::Init();

  KURL::Initialize();
  SchemeRegistry::Initialize();
  SecurityPolicy::Init();

  RegisterEventFactory();

  StringImpl::FreezeStaticStrings();

  V8ThrowDOMException::Init();

  BindingSecurity::Init();

  TimeZoneController::Init();
}

}  // namespace blink
