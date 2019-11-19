/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_PENDING_SCRIPT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_PENDING_SCRIPT_H_

#include "base/macros.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink-forward.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"
#include "third_party/blink/renderer/core/script/script_scheduling_type.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

namespace blink {

class PendingScript;

class CORE_EXPORT PendingScriptClient : public GarbageCollectedMixin {
 public:
  virtual ~PendingScriptClient() {}

  // Invoked when the pending script is ready. This could be during
  // WatchForLoad() (if the pending script was already ready), or when the
  // resource loads (if script streaming is not occurring), or when script
  // streaming finishes.
  virtual void PendingScriptFinished(PendingScript*) = 0;

  void Trace(Visitor* visitor) override {}
};

// A container for an script after "prepare a script" until it is executed.
// ScriptLoader creates a PendingScript in ScriptLoader::PrepareScript(), and
// a Script is created via PendingScript::GetSource() when it becomes ready.
// When "script is ready"
// https://html.spec.whatwg.org/C/#the-script-is-ready,
// PendingScriptClient is notified.
class CORE_EXPORT PendingScript : public GarbageCollected<PendingScript>,
                                  public NameClient {
 public:
  virtual ~PendingScript();

  TextPosition StartingPosition() const { return starting_position_; }
  void MarkParserBlockingLoadStartTime();
  // Returns the time the load of this script started blocking the parser, or
  // zero if this script hasn't yet blocked the parser, in
  // monotonicallyIncreasingTime.
  base::TimeTicks ParserBlockingLoadStartTime() const {
    return parser_blocking_load_start_time_;
  }

  virtual void WatchForLoad(PendingScriptClient*);
  void StopWatchingForLoad();
  void PendingScriptFinished();

  ScriptElementBase* GetElement() const;

  virtual mojom::ScriptType GetScriptType() const = 0;

  virtual void Trace(Visitor*);
  const char* NameInHeapSnapshot() const override { return "PendingScript"; }

  // Returns nullptr when "script's script is null", i.e. an error occurred.
  virtual Script* GetSource(const KURL& document_url) const = 0;

  // https://html.spec.whatwg.org/C/#the-script-is-ready
  virtual bool IsReady() const = 0;
  virtual bool IsExternal() const = 0;
  virtual bool WasCanceled() const = 0;

  // Support for script streaming.
  virtual void StartStreamingIfPossible() = 0;

  // Used only for tracing, and can return a null URL.
  // TODO(hiroshige): It's preferable to return the base URL consistently
  // https://html.spec.whatwg.org/C/#concept-script-base-url
  // but it requires further refactoring.
  virtual KURL UrlForTracing() const = 0;

  // Used for DCHECK()s.
  bool IsExternalOrModule() const {
    return IsExternal() || GetScriptType() == mojom::ScriptType::kModule;
  }

  void Dispose();

  ScriptSchedulingType GetSchedulingType() const {
    DCHECK_NE(scheduling_type_, ScriptSchedulingType::kNotSet);
    return scheduling_type_;
  }
  bool IsControlledByScriptRunner() const;
  void SetSchedulingType(ScriptSchedulingType scheduling_type) {
    DCHECK_EQ(scheduling_type_, ScriptSchedulingType::kNotSet);
    scheduling_type_ = scheduling_type;
  }
  Document* OriginalContextDocument() const {
    return original_context_document_;
  }

  bool WasCreatedDuringDocumentWrite() {
    return created_during_document_write_;
  }

  // https://html.spec.whatwg.org/C/#execute-the-script-block
  // The single entry point of script execution.
  // PendingScript::Dispose() is called in ExecuteScriptBlock().
  //
  // This is virtual only for testing.
  virtual void ExecuteScriptBlock(const KURL&);

 protected:
  PendingScript(ScriptElementBase*, const TextPosition& starting_position);

  virtual void DisposeInternal() = 0;

  PendingScriptClient* Client() { return client_; }
  bool IsWatchingForLoad() const { return client_; }

  virtual void CheckState() const = 0;

 private:
  static void ExecuteScriptBlockInternal(
      Script*,
      ScriptElementBase*,
      bool was_canceled,
      bool is_external,
      bool created_during_document_write,
      base::TimeTicks parser_blocking_load_start_time,
      bool is_controlled_by_script_runner);

  // |m_element| must points to the corresponding ScriptLoader's
  // ScriptElementBase and thus must be non-null before dispose() is called
  // (except for unit tests).
  Member<ScriptElementBase> element_;

  TextPosition starting_position_;  // Only used for inline script tags.
  base::TimeTicks parser_blocking_load_start_time_;

  ScriptSchedulingType scheduling_type_ = ScriptSchedulingType::kNotSet;

  WebScopedVirtualTimePauser virtual_time_pauser_;
  Member<PendingScriptClient> client_;

  // The context/element document at the time when PrepareScript() is executed.
  // These are only used to check whether the script element is moved between
  // documents and thus don't retain a strong references.
  WeakMember<Document> original_element_document_;
  WeakMember<Document> original_context_document_;

  const bool created_during_document_write_;

  DISALLOW_COPY_AND_ASSIGN(PendingScript);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_PENDING_SCRIPT_H_
