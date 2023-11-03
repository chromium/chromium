// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_context_snapshot.h"

namespace blink {

namespace {

V8ContextSnapshot::CreateContextFromSnapshotFuncType
    g_create_context_from_snapshot_func;
V8ContextSnapshot::InstallContextIndependentPropsFuncType
    g_install_context_independent_props_func;
V8ContextSnapshot::EnsureInterfaceTemplatesFuncType
    g_ensure_interface_templates_func;
V8ContextSnapshot::TakeSnapshotFuncType g_take_snapshot_func;
V8ContextSnapshot::GetReferenceTableFuncType g_get_reference_table_func;

}  // namespace

v8::Local<v8::Context> V8ContextSnapshot::CreateContextFromSnapshot(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::ExtensionConfiguration* extension_config,
    v8::Local<v8::Object> global_proxy,
    Document* document) {
  return g_create_context_from_snapshot_func(isolate, world, extension_config,
                                             global_proxy, document);
}

void V8ContextSnapshot::InstallContextIndependentProps(
    ScriptState* script_state) {
  return g_install_context_independent_props_func(script_state);
}

void V8ContextSnapshot::EnsureInterfaceTemplates(v8::Isolate* isolate) {
  return g_ensure_interface_templates_func(isolate);
}

v8::StartupData V8ContextSnapshot::TakeSnapshot(v8::Isolate* isolate) {
  return g_take_snapshot_func(isolate);
}

const intptr_t* V8ContextSnapshot::GetReferenceTable() {
  return g_get_reference_table_func();
}

void V8ContextSnapshot::SetCreateContextFromSnapshotFunc(
    CreateContextFromSnapshotFuncType func) {
  DCHECK(!g_create_context_from_snapshot_func);
  DCHECK(func);
  g_create_context_from_snapshot_func = func;
}

void V8ContextSnapshot::SetInstallContextIndependentPropsFunc(
    InstallContextIndependentPropsFuncType func) {
  DCHECK(!g_install_context_independent_props_func);
  DCHECK(func);
  g_install_context_independent_props_func = func;
}

void V8ContextSnapshot::SetEnsureInterfaceTemplatesFunc(
    EnsureInterfaceTemplatesFuncType func) {
  DCHECK(!g_ensure_interface_templates_func);
  DCHECK(func);
  g_ensure_interface_templates_func = func;
}

void V8ContextSnapshot::SetTakeSnapshotFunc(TakeSnapshotFuncType func) {
  DCHECK(!g_take_snapshot_func);
  DCHECK(func);
  g_take_snapshot_func = func;
}

void V8ContextSnapshot::SetGetReferenceTableFunc(
    GetReferenceTableFuncType func) {
  DCHECK(!g_get_reference_table_func);
  DCHECK(func);
  g_get_reference_table_func = func;
}

}  // namespace blink
