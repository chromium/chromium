// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_event_recorder.h"

#include <atk/atk.h>
#include <atk/atkutil.h>
#include <atspi/atspi.h>

#include "base/process/process_handle.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_auralinux.h"
#include "content/browser/accessibility/browser_accessibility_auralinux.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 16, 0)
#define ATK_216
#endif

namespace content {

// This class has two distinct event recording code paths. When we are
// recording events in-process (typically this is used for
// DumpAccessibilityEvents tests), we use ATK's global event handlers. Since
// ATK doesn't support intercepting events from other processes, if we have a
// non-zero PID or an accessibility application name pattern, we use AT-SPI2
// directly to intercept events. Since AT-SPI2 should be capable of
// intercepting events in-process as well, eventually it would be nice to
// remove the ATK code path entirely.
class AccessibilityEventRecorderAuraLinux : public AccessibilityEventRecorder {
 public:
  explicit AccessibilityEventRecorderAuraLinux(
      BrowserAccessibilityManager* manager,
      base::ProcessId pid,
      const base::StringPiece& application_name_match_pattern);
  ~AccessibilityEventRecorderAuraLinux() override;

  void ProcessATKEvent(const char* event,
                       unsigned int n_params,
                       const GValue* params);
  void ProcessATSPIEvent(const AtspiEvent* event);

  static gboolean OnATKEventReceived(GSignalInvocationHint* hint,
                                     unsigned int n_params,
                                     const GValue* params,
                                     gpointer data);

 private:
  bool ShouldUseATSPI();

  void AddATKEventListener(const char* event_name);
  void AddATKEventListeners();
  void RemoveATKEventListeners();
  bool IncludeState(AtkStateType state_type);

  void AddATSPIEventListeners();
  void RemoveATSPIEventListeners();

  AtspiEventListener* atspi_event_listener_ = nullptr;
  base::ProcessId pid_;
  base::StringPiece application_name_match_pattern_;
  std::vector<unsigned int> atk_listener_ids_;
  static AccessibilityEventRecorderAuraLinux* instance_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityEventRecorderAuraLinux);
};

// static
AccessibilityEventRecorderAuraLinux*
    AccessibilityEventRecorderAuraLinux::instance_ = nullptr;

// static
gboolean AccessibilityEventRecorderAuraLinux::OnATKEventReceived(
    GSignalInvocationHint* hint,
    unsigned int n_params,
    const GValue* params,
    gpointer data) {
  GSignalQuery query;
  g_signal_query(hint->signal_id, &query);

  if (instance_) {
    instance_->ProcessATKEvent(query.signal_name, n_params, params);
  }

  return true;
}

// static
std::unique_ptr<AccessibilityEventRecorder> AccessibilityEventRecorder::Create(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const base::StringPiece& application_name_match_pattern) {
  return std::make_unique<AccessibilityEventRecorderAuraLinux>(
      manager, pid, application_name_match_pattern);
}

bool AccessibilityEventRecorderAuraLinux::ShouldUseATSPI() {
  return pid_ != base::GetCurrentProcId() ||
         !application_name_match_pattern_.empty();
}

AccessibilityEventRecorderAuraLinux::AccessibilityEventRecorderAuraLinux(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const base::StringPiece& application_name_match_pattern)
    : AccessibilityEventRecorder(manager),
      pid_(pid),
      application_name_match_pattern_(application_name_match_pattern) {
  CHECK(!instance_) << "There can be only one instance of"
                    << " AccessibilityEventRecorder at a time.";

  if (ShouldUseATSPI()) {
    AddATSPIEventListeners();
  } else {
    AddATKEventListeners();
  }

  instance_ = this;
}

AccessibilityEventRecorderAuraLinux::~AccessibilityEventRecorderAuraLinux() {
  RemoveATSPIEventListeners();
  instance_ = nullptr;
}

void AccessibilityEventRecorderAuraLinux::AddATKEventListener(
    const char* event_name) {
  unsigned id = atk_add_global_event_listener(OnATKEventReceived, event_name);
  if (!id)
    LOG(FATAL) << "atk_add_global_event_listener failed for " << event_name;

  atk_listener_ids_.push_back(id);
}

void AccessibilityEventRecorderAuraLinux::AddATKEventListeners() {
  GObject* gobject = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr, nullptr));
  g_object_unref(atk_no_op_object_new(gobject));
  g_object_unref(gobject);

  AddATKEventListener("ATK:AtkObject:state-change");
  AddATKEventListener("ATK:AtkObject:focus-event");
  AddATKEventListener("ATK:AtkObject:property-change");
}

void AccessibilityEventRecorderAuraLinux::RemoveATKEventListeners() {
  for (const auto& id : atk_listener_ids_)
    atk_remove_global_event_listener(id);

  atk_listener_ids_.clear();
}

// Pruning states which are not supported on older bots makes it possible to
// run the events tests in more environments.
bool AccessibilityEventRecorderAuraLinux::IncludeState(
    AtkStateType state_type) {
  switch (state_type) {
#if defined(ATK_216)
    case ATK_STATE_CHECKABLE:
    case ATK_STATE_HAS_POPUP:
    case ATK_STATE_READ_ONLY:
      return false;
#endif
    case ATK_STATE_LAST_DEFINED:
      return false;
    default:
      return true;
  }
}

void AccessibilityEventRecorderAuraLinux::ProcessATKEvent(
    const char* event,
    unsigned int n_params,
    const GValue* params) {
  // If we don't have a root object, it means the tree is being destroyed.
  if (!manager_->GetRoot()) {
    RemoveATKEventListeners();
    return;
  }

  std::string log;
  if (std::string(event).find("property-change") != std::string::npos) {
    DCHECK_GE(n_params, 2u);
    AtkPropertyValues* property_values =
        static_cast<AtkPropertyValues*>(g_value_get_pointer(&params[1]));

    if (g_strcmp0(property_values->property_name, "accessible-value"))
      return;

    log += "VALUE-CHANGED:";
    log +=
        base::NumberToString(g_value_get_double(&property_values->new_value));
  } else {
    log += base::ToUpperASCII(event);
    if (std::string(event).find("state-change") != std::string::npos) {
      log += ":" + base::ToUpperASCII(g_value_get_string(&params[1]));
      log += base::StringPrintf(":%s", g_strdup_value_contents(&params[2]));
    }
  }

  AtkObject* obj = ATK_OBJECT(g_value_get_object(&params[0]));
  std::string role = atk_role_get_name(atk_object_get_role(obj));
  base::ReplaceChars(role, " ", "_", &role);
  log += base::StringPrintf(" role=ROLE_%s", base::ToUpperASCII(role).c_str());
  log += base::StringPrintf(" name='%s'", atk_object_get_name(obj));

  std::string states = "";
  AtkStateSet* state_set = atk_object_ref_state_set(obj);
  for (int i = ATK_STATE_INVALID; i < ATK_STATE_LAST_DEFINED; i++) {
    AtkStateType state_type = static_cast<AtkStateType>(i);
    if (atk_state_set_contains_state(state_set, state_type) &&
        IncludeState(state_type)) {
      states += " " + base::ToUpperASCII(atk_state_type_get_name(state_type));
    }
  }
  states = base::CollapseWhitespaceASCII(states, false);
  base::ReplaceChars(states, " ", ",", &states);
  log += base::StringPrintf(" %s", states.c_str());

  OnEvent(log);
}

// This list is composed of the sorted event names taken from the list provided
// in the libatspi documentation at:
// https://developer.gnome.org/libatspi/stable/AtspiEventListener.html#atspi-event-listener-register
const char* const kEventNames[] = {
    "object:active-descendant-changed",
    "object:children-changed",
    "object:column-deleted",
    "object:column-inserted",
    "object:column-reordered",
    "object:model-changed",
    "object:property-change",
    "object:property-change:accessible-description",
    "object:property-change:accessible-name",
    "object:property-change:accessible-parent",
    "object:property-change:accessible-role",
    "object:property-change:accessible-table-caption",
    "object:property-change:accessible-table-column-description",
    "object:property-change:accessible-table-column-header",
    "object:property-change:accessible-table-row-description",
    "object:property-change:accessible-table-row-header",
    "object:property-change:accessible-table-summary",
    "object:property-change:accessible-value",
    "object:row-deleted",
    "object:row-inserted",
    "object:row-reordered",
    "object:selection-changed",
    "object:state-changed",
    "object:text-caret-moved",
    "object:text-changed",
    "object:text-selection-changed",
    "object:visible-data-changed",
    "window:activate",
    "window:close",
    "window:create",
    "window:deactivate",
    "window:desktop-create",
    "window:desktop-destroy",
    "window:lower",
    "window:maximize",
    "window:minimize",
    "window:move",
    "window:raise",
    "window:reparent",
    "window:resize",
    "window:restore",
    "window:restyle",
    "window:shade",
    "window:unshade",
};

static void OnATSPIEventReceived(AtspiEvent* event, void* data) {
  static_cast<AccessibilityEventRecorderAuraLinux*>(data)->ProcessATSPIEvent(
      event);
  g_boxed_free(ATSPI_TYPE_EVENT, static_cast<void*>(event));
}

void AccessibilityEventRecorderAuraLinux::AddATSPIEventListeners() {
  atspi_init();
  atspi_event_listener_ =
      atspi_event_listener_new(OnATSPIEventReceived, this, nullptr);

  GError* error = nullptr;
  for (size_t i = 0; i < base::size(kEventNames); i++) {
    atspi_event_listener_register(atspi_event_listener_, kEventNames[i],
                                  &error);
    if (error) {
      LOG(ERROR) << "Could not register event listener for " << kEventNames[i];
      g_clear_error(&error);
    }
  }
}

void AccessibilityEventRecorderAuraLinux::RemoveATSPIEventListeners() {
  if (!atspi_event_listener_)
    return;

  GError* error = nullptr;
  for (size_t i = 0; i < base::size(kEventNames); i++) {
    atspi_event_listener_deregister(atspi_event_listener_, kEventNames[i],
                                    nullptr);
    if (error) {
      LOG(ERROR) << "Could not deregister event listener for "
                 << kEventNames[i];
      g_clear_error(&error);
    }
  }

  g_object_unref(atspi_event_listener_);
  atspi_event_listener_ = nullptr;
}

void AccessibilityEventRecorderAuraLinux::ProcessATSPIEvent(
    const AtspiEvent* event) {
  GError* error = nullptr;

  if (!application_name_match_pattern_.empty()) {
    AtspiAccessible* application =
        atspi_accessible_get_application(event->source, &error);
    if (error || !application)
      return;

    char* application_name = atspi_accessible_get_name(application, &error);
    g_object_unref(application);
    if (error || !application_name) {
      g_clear_error(&error);
      return;
    }

    if (!base::MatchPattern(application_name,
                            application_name_match_pattern_)) {
      return;
    }
    free(application_name);
  }

  if (pid_) {
    int pid = atspi_accessible_get_process_id(event->source, &error);
    if (!error && pid != pid_)
      return;
    g_clear_error(&error);
  }

  std::stringstream output;
  output << event->type << " ";

  GHashTable* attributes =
      atspi_accessible_get_attributes(event->source, &error);
  std::string html_tag, html_class, html_id;
  if (!error && attributes) {
    if (char* tag = static_cast<char*>(g_hash_table_lookup(attributes, "tag")))
      html_tag = tag;
    if (char* id = static_cast<char*>(g_hash_table_lookup(attributes, "id")))
      html_id = id;
    if (char* class_chars =
            static_cast<char*>(g_hash_table_lookup(attributes, "class")))
      html_class = std::string(".") + class_chars;
    g_hash_table_unref(attributes);
  }
  g_clear_error(&error);

  if (!html_tag.empty())
    output << "<" << html_tag << html_id << html_class << ">";

  AtspiRole role = atspi_accessible_get_role(event->source, &error);
  output << "role=";
  if (!error)
    output << ATSPIRoleToString(role);
  else
    output << "#error";
  g_clear_error(&error);

  char* name = atspi_accessible_get_name(event->source, &error);
  output << " name=";
  if (!error && name)
    output << name;
  else
    output << "#error";
  g_clear_error(&error);
  free(name);

  AtspiStateSet* atspi_states = atspi_accessible_get_state_set(event->source);
  GArray* state_array = atspi_state_set_get_states(atspi_states);
  std::vector<std::string> states;
  for (unsigned i = 0; i < state_array->len; i++) {
    AtspiStateType state_type = g_array_index(state_array, AtspiStateType, i);
    states.push_back(ATSPIStateToString(state_type));
  }
  g_array_free(state_array, TRUE);
  g_object_unref(atspi_states);
  output << " ";
  std::copy(states.begin(), states.end(),
            std::ostream_iterator<std::string>(output, ", "));

  OnEvent(output.str());
}

}  // namespace content
