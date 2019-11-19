/*
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_LOADER_H_

#include "third_party/blink/public/mojom/script/script_type.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/script/pending_script.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/script/script_runner.h"
#include "third_party/blink/renderer/core/script/script_scheduling_type.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Resource;
class ResourceFetcher;
class ScriptElementBase;
class Script;
class ScriptResource;
class Modulator;

class CORE_EXPORT ScriptLoader final : public GarbageCollected<ScriptLoader>,
                                       public PendingScriptClient,
                                       public NameClient {
  USING_GARBAGE_COLLECTED_MIXIN(ScriptLoader);

 public:
  ScriptLoader(ScriptElementBase*, bool created_by_parser, bool is_evaluated);
  ~ScriptLoader() override;
  void Trace(Visitor*) override;
  const char* NameInHeapSnapshot() const override { return "ScriptLoader"; }

  enum LegacyTypeSupport {
    kDisallowLegacyTypeInTypeAttribute,
    kAllowLegacyTypeInTypeAttribute
  };

  // |out_is_import_map| is set separately from |out_script_type| in order
  // to avoid adding import maps as a mojom::ScriptType enum, because import
  // maps are processed quite differently from classic/module scripts.
  //
  // TODO(hiroshige, kouhei): Make the method signature simpler.
  static bool IsValidScriptTypeAndLanguage(
      const String& type_attribute_value,
      const String& language_attribute_value,
      LegacyTypeSupport support_legacy_types,
      mojom::ScriptType* out_script_type = nullptr,
      bool* out_is_import_map = nullptr);

  static bool BlockForNoModule(mojom::ScriptType, bool nomodule);

  static network::mojom::CredentialsMode ModuleScriptCredentialsMode(
      CrossOriginAttributeValue);

  // https://html.spec.whatwg.org/C/#prepare-a-script
  bool PrepareScript(const TextPosition& script_start_position =
                         TextPosition::MinimumPosition(),
                     LegacyTypeSupport = kDisallowLegacyTypeInTypeAttribute);

  // Gets a PendingScript for external script whose fetch is started in
  // FetchClassicScript()/FetchModuleScriptTree().
  // This should be called only once.
  PendingScript* TakePendingScript(ScriptSchedulingType);

  bool WillBeParserExecuted() const { return will_be_parser_executed_; }
  bool ReadyToBeParserExecuted() const { return ready_to_be_parser_executed_; }
  bool WillExecuteWhenDocumentFinishedParsing() const {
    return will_execute_when_document_finished_parsing_;
  }
  bool IsForceDeferred() const { return force_deferred_; }
  bool IsParserInserted() const { return parser_inserted_; }
  bool AlreadyStarted() const { return already_started_; }
  bool IsNonBlocking() const { return non_blocking_; }
  mojom::ScriptType GetScriptType() const { return script_type_; }

  // Helper functions used by our parent classes.
  void DidNotifySubtreeInsertionsToDocument();
  void ChildrenChanged();
  void HandleSourceAttribute(const String& source_url);
  void HandleAsyncAttribute();

  void SetFetchDocWrittenScriptDeferIdle();

  // Return non-null if controlled by ScriptRunner, or null otherwise.
  // Only for ScriptRunner::MovePendingScript() and should be removed once
  // crbug.com/721914 is fixed.
  PendingScript* GetPendingScriptIfControlledByScriptRunnerForCrossDocMove();

 private:
  bool IgnoresLoadRequest() const;
  bool IsScriptForEventSupported() const;

  // FetchClassicScript corresponds to Step 21.6 of
  // https://html.spec.whatwg.org/C/#prepare-a-script
  // and must NOT be called from outside of PendingScript().
  //
  // https://html.spec.whatwg.org/C/#fetch-a-classic-script
  void FetchClassicScript(const KURL&,
                          Document&,
                          const ScriptFetchOptions&,
                          CrossOriginAttributeValue,
                          const WTF::TextEncoding&);
  // https://html.spec.whatwg.org/C/#fetch-a-module-script-tree
  void FetchModuleScriptTree(const KURL&,
                             ResourceFetcher*,
                             Modulator*,
                             const ScriptFetchOptions&);

  // Clears the connection to the PendingScript.
  void DetachPendingScript();

  // PendingScriptClient
  void PendingScriptFinished(PendingScript*) override;

  Member<ScriptElementBase> element_;

  // https://html.spec.whatwg.org/C/#script-processing-model
  // "A script element has several associated pieces of state.":

  // <spec href="https://html.spec.whatwg.org/C/#already-started">... Initially,
  // script elements must have this flag unset ...</spec>
  bool already_started_ = false;

  // <spec href="https://html.spec.whatwg.org/C/#parser-inserted">... Initially,
  // script elements must have this flag unset. ...</spec>
  bool parser_inserted_ = false;

  // <spec href="https://html.spec.whatwg.org/C/#non-blocking">... Initially,
  // script elements must have this flag set. ...</spec>
  bool non_blocking_ = true;

  // <spec href="https://html.spec.whatwg.org/C/#ready-to-be-parser-executed">
  // ... Initially, script elements must have this flag unset ...</spec>
  bool ready_to_be_parser_executed_ = false;

  // <spec href="https://html.spec.whatwg.org/C/#concept-script-type">... It is
  // determined when the script is prepared, ...</spec>
  mojom::ScriptType script_type_ = mojom::ScriptType::kClassic;

  // <spec href="https://html.spec.whatwg.org/C/#concept-script-external">
  // ... It is determined when the script is prepared, ...</spec>
  bool is_external_script_ = false;

  // Same as "The parser will handle executing the script."
  bool will_be_parser_executed_;

  bool will_execute_when_document_finished_parsing_;

  // The script will be force deferred (https://crbug.com/976061).
  bool force_deferred_;

  // A PendingScript is first created in PrepareScript() and stored in
  // |prepared_pending_script_|.
  // Later, TakePendingScript() is called, and its caller holds a reference
  // to the PendingScript instead and |prepared_pending_script_| is cleared.
  Member<PendingScript> prepared_pending_script_;

  // If the script is controlled by ScriptRunner, then
  // ScriptLoader::pending_script_ holds a reference to the PendingScript and
  // ScriptLoader is its client.
  // Otherwise, HTMLParserScriptRunner or XMLParserScriptRunner holds the
  // reference and |pending_script_| here is null.
  Member<PendingScript> pending_script_;

  // This is used only to keep the ScriptResource of a classic script alive
  // and thus to keep it on MemoryCache, even after script execution, as long
  // as ScriptLoader is alive. crbug.com/778799
  Member<Resource> resource_keep_alive_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_LOADER_H_
