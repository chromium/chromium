// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/webui/sync_internals/sync_internals_message_handler.h"

#import <utility>
#import <vector>

#import "base/command_line.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/values.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/weak_handle.h"
#import "components/sync/engine/events/protocol_event.h"
#import "components/sync/invalidations/sync_invalidations_service.h"
#import "components/sync/model/type_entities_count.h"
#import "components/sync/protocol/sync_invalidations_payload.pb.h"
#import "components/sync/service/sync_internals_util.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/components/webui/web_ui_provider.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/webui/web_ui_ios.h"

namespace {

// Returns the initial state of the "include specifics" flag, based on whether
// or not the corresponding command-line switch is set.
bool GetIncludeSpecificsInitialState() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      syncer::kSyncIncludeSpecificsInProtocolLog);
}

}  // namespace

SyncInternalsMessageHandler::SyncInternalsMessageHandler()
    : include_specifics_(GetIncludeSpecificsInitialState()),
      weak_ptr_factory_(this) {}

SyncInternalsMessageHandler::~SyncInternalsMessageHandler() {
  syncer::SyncService* service = GetSyncService();
  if (service && service->HasObserver(this)) {
    service->RemoveObserver(this);
    service->RemoveProtocolEventObserver(this);
  }

  if (is_registered_) {
    syncer::SyncInvalidationsService* invalidations_service =
        GetSyncInvalidationsService();
    if (invalidations_service) {
      invalidations_service->RemoveListener(this);
    }
  }
}

void SyncInternalsMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestDataAndRegisterForUpdates,
      base::BindRepeating(
          &SyncInternalsMessageHandler::HandleRequestDataAndRegisterForUpdates,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestListOfTypes,
      base::BindRepeating(
          &SyncInternalsMessageHandler::HandleRequestListOfTypes,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestIncludeSpecificsInitialState,
      base::BindRepeating(&SyncInternalsMessageHandler::
                              HandleRequestIncludeSpecificsInitialState,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kSetIncludeSpecifics,
      base::BindRepeating(
          &SyncInternalsMessageHandler::HandleSetIncludeSpecifics,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestStart,
      base::BindRepeating(&SyncInternalsMessageHandler::HandleRequestStart,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestStopClearData,
      base::BindRepeating(
          &SyncInternalsMessageHandler::HandleRequestStopClearData,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kTriggerRefresh,
      base::BindRepeating(&SyncInternalsMessageHandler::HandleTriggerRefresh,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kGetAllNodes,
      base::BindRepeating(&SyncInternalsMessageHandler::HandleGetAllNodes,
                          base::Unretained(this)));
}

void SyncInternalsMessageHandler::HandleRequestDataAndRegisterForUpdates(
    const base::Value::List& args) {
  DCHECK(args.empty());

  // is_registered_ flag protects us from double-registering.  This could
  // happen on a page refresh, where the JavaScript gets re-run but the
  // message handler remains unchanged.
  syncer::SyncService* service = GetSyncService();
  if (service && !is_registered_) {
    service->AddObserver(this);
    service->AddProtocolEventObserver(this);
    syncer::SyncInvalidationsService* invalidations_service =
        GetSyncInvalidationsService();
    if (invalidations_service) {
      invalidations_service->AddListener(this);
    }
    is_registered_ = true;
  }

  SendAboutInfoAndEntityCounts();
}

void SyncInternalsMessageHandler::HandleRequestListOfTypes(
    const base::Value::List& args) {
  DCHECK(args.empty());
  base::Value::Dict event_details;
  base::Value::List type_list;
  syncer::ModelTypeSet protocol_types = syncer::ProtocolTypes();
  for (syncer::ModelType type : protocol_types) {
    type_list.Append(ModelTypeToDebugString(type));
  }
  event_details.Set(syncer::sync_ui_util::kTypes, std::move(type_list));
  DispatchEvent(syncer::sync_ui_util::kOnReceivedListOfTypes, event_details);
}

void SyncInternalsMessageHandler::HandleRequestIncludeSpecificsInitialState(
    const base::Value::List& args) {
  DCHECK(args.empty());

  base::Value::Dict value;
  value.Set(syncer::sync_ui_util::kIncludeSpecifics,
            GetIncludeSpecificsInitialState());

  DispatchEvent(syncer::sync_ui_util::kOnReceivedIncludeSpecificsInitialState,
                value);
}

void SyncInternalsMessageHandler::HandleGetAllNodes(
    const base::Value::List& args) {
  DCHECK_EQ(1U, args.size());
  DCHECK(args[0].is_string());
  std::string callback_id = args[0].GetString();

  syncer::SyncService* service = GetSyncService();
  if (service) {
    service->GetAllNodesForDebugging(
        base::BindOnce(&SyncInternalsMessageHandler::OnReceivedAllNodes,
                       weak_ptr_factory_.GetWeakPtr(), callback_id));
  }
}

void SyncInternalsMessageHandler::HandleSetIncludeSpecifics(
    const base::Value::List& args) {
  DCHECK_EQ(1U, args.size());
  include_specifics_ = args[0].GetBool();
}

void SyncInternalsMessageHandler::HandleRequestStart(
    const base::Value::List& args) {
  DCHECK_EQ(0U, args.size());

  syncer::SyncService* service = GetSyncService();
  if (!service) {
    return;
  }

  service->SetSyncFeatureRequested();
  // If the service was previously stopped with CLEAR_DATA, then the
  // "first-setup-complete" bit was also cleared, and now the service wouldn't
  // fully start up. So set that too.
  service->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
}

void SyncInternalsMessageHandler::HandleRequestStopClearData(
    const base::Value::List& args) {
  DCHECK_EQ(0U, args.size());

  syncer::SyncService* service = GetSyncService();
  if (!service) {
    return;
  }

  service->StopAndClear();
}

void SyncInternalsMessageHandler::HandleTriggerRefresh(
    const base::Value::List& args) {
  syncer::SyncService* service = GetSyncService();
  if (!service) {
    return;
  }

  service->TriggerRefresh(syncer::ModelTypeSet::All());
}

void SyncInternalsMessageHandler::OnReceivedAllNodes(
    const std::string& callback_id,
    base::Value::List nodes) {
  base::Value id(callback_id);
  base::Value success(true);

  base::ValueView args[] = {id, success, nodes};
  web_ui()->CallJavascriptFunction("cr.webUIResponse", args);
}

void SyncInternalsMessageHandler::OnStateChanged(syncer::SyncService* sync) {
  SendAboutInfoAndEntityCounts();
}

void SyncInternalsMessageHandler::OnProtocolEvent(
    const syncer::ProtocolEvent& event) {
  DispatchEvent(syncer::sync_ui_util::kOnProtocolEvent,
                event.ToValue(include_specifics_));
}

void SyncInternalsMessageHandler::OnInvalidationReceived(
    const std::string& payload) {
  sync_pb::SyncInvalidationsPayload payload_message;
  if (!payload_message.ParseFromString(payload)) {
    return;
  }

  base::Value::List data_types_list;
  for (const auto& data_type_invalidation :
       payload_message.data_type_invalidations()) {
    const int field_number = data_type_invalidation.data_type_id();
    syncer::ModelType type =
        syncer::GetModelTypeFromSpecificsFieldNumber(field_number);
    if (IsRealDataType(type)) {
      data_types_list.Append(syncer::ModelTypeToDebugString(type));
    }
  }

  DispatchEvent(syncer::sync_ui_util::kOnInvalidationReceived, data_types_list);
}

void SyncInternalsMessageHandler::SendAboutInfoAndEntityCounts() {
  // This class serves to display debug information to the user, so it's fine to
  // include sensitive data in ConstructAboutInformation().
  base::Value::Dict value = syncer::sync_ui_util::ConstructAboutInformation(
      syncer::sync_ui_util::IncludeSensitiveData(true), GetSyncService(),
      web_ui::GetChannelString());
  DispatchEvent(syncer::sync_ui_util::kOnAboutInfoUpdated, value);

  if (syncer::SyncService* service = GetSyncService()) {
    service->GetEntityCountsForDebugging(
        BindOnce(&SyncInternalsMessageHandler::OnGotEntityCounts,
                 weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnGotEntityCounts({});
  }
}

void SyncInternalsMessageHandler::OnGotEntityCounts(
    const std::vector<syncer::TypeEntitiesCount>& entity_counts) {
  base::Value::List count_list;
  for (const syncer::TypeEntitiesCount& count : entity_counts) {
    base::Value::Dict count_dictionary;
    count_dictionary.Set(syncer::sync_ui_util::kModelType,
                         ModelTypeToDebugString(count.type));
    count_dictionary.Set(syncer::sync_ui_util::kEntities, count.entities);
    count_dictionary.Set(syncer::sync_ui_util::kNonTombstoneEntities,
                         count.non_tombstone_entities);
    count_list.Append(std::move(count_dictionary));
  }

  base::Value::Dict event_details;
  event_details.Set(syncer::sync_ui_util::kEntityCounts, std::move(count_list));
  DispatchEvent(syncer::sync_ui_util::kOnEntityCountsUpdated, event_details);
}

// Gets the SyncService of the underlying original profile. May return null.
syncer::SyncService* SyncInternalsMessageHandler::GetSyncService() {
  return web_ui::GetSyncServiceForWebUI(web_ui());
}

syncer::SyncInvalidationsService*
SyncInternalsMessageHandler::GetSyncInvalidationsService() {
  return web_ui::GetSyncInvalidationsServiceForWebUI(web_ui());
}

void SyncInternalsMessageHandler::DispatchEvent(
    const std::string& name,
    const base::ValueView details_value) {
  base::Value event_name = base::Value(name);
  base::ValueView args[] = {event_name, details_value};
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback", args);
}
