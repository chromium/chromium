// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/document_metadata.h"

#include <optional>

#include "content/public/browser/render_frame_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content::webid {

namespace {

constexpr char kLoginActionType[] = "LoginAction";
constexpr char kFederationProperty[] = "federation";
constexpr char kProvidersProperty[] = "providers";
constexpr char kConfigUrlProperty[] = "configURL";
constexpr char kClientIdProperty[] = "clientId";
constexpr char kNonceProperty[] = "nonce";
constexpr char kFieldsProperty[] = "fields";

const schema_org::mojom::EntityPtr GetEntityProperty(
    const schema_org::mojom::Entity& entity,
    const std::string& name) {
  for (const auto& property : entity.properties) {
    if (property->name == name && property->values->is_entity_values() &&
        !property->values->get_entity_values().empty()) {
      return property->values->get_entity_values()[0].Clone();
    }
  }
  return nullptr;
}

std::optional<std::string> GetStringProperty(
    const schema_org::mojom::EntityPtr& entity,
    const std::string& name) {
  if (!entity) {
    return std::nullopt;
  }
  for (const auto& property : entity->properties) {
    if (property->name == name && property->values->is_string_values() &&
        !property->values->get_string_values().empty()) {
      return property->values->get_string_values()[0];
    }
  }
  return std::nullopt;
}

std::optional<std::vector<std::string>> GetStringArrayProperty(
    const schema_org::mojom::EntityPtr& entity,
    const std::string& name) {
  if (!entity) {
    return std::nullopt;
  }
  for (const auto& property : entity->properties) {
    if (property->name == name && property->values->is_string_values()) {
      return property->values->get_string_values();
    }
  }
  return std::nullopt;
}

std::optional<blink::mojom::IdentityProviderGetParametersPtr> Parse(
    const schema_org::mojom::Entity& entity) {
  if (entity.type != kLoginActionType) {
    return std::nullopt;
  }

  auto federation = GetEntityProperty(entity, kFederationProperty);
  if (!federation) {
    return std::nullopt;
  }

  auto params = blink::mojom::IdentityProviderGetParameters::New();
  params->context = blink::mojom::RpContext::kSignIn;
  params->mode = blink::mojom::RpMode::kPassive;

  for (const auto& property : federation->properties) {
    if (property->name == kProvidersProperty &&
        property->values->is_entity_values()) {
      for (const auto& provider_entity :
           property->values->get_entity_values()) {
        auto options = blink::mojom::IdentityProviderRequestOptions::New();
        options->config = blink::mojom::IdentityProviderConfig::New();

        // TODO(crbug.com/477699742): validate that the necessary fields
        // are present and well-formed.
        auto config_url =
            GetStringProperty(provider_entity, kConfigUrlProperty);
        if (config_url) {
          options->config->config_url = GURL(*config_url);
        }

        auto client_id = GetStringProperty(provider_entity, kClientIdProperty);
        if (client_id) {
          options->config->client_id = *client_id;
        }

        auto nonce = GetStringProperty(provider_entity, kNonceProperty);
        if (nonce) {
          options->nonce = *nonce;
        }

        options->fields =
            GetStringArrayProperty(provider_entity, kFieldsProperty);

        if (options->config->config_url.is_valid()) {
          params->providers.push_back(std::move(options));
        }
      }
    }
  }

  if (params->providers.empty()) {
    return std::nullopt;
  }

  return std::move(params);
}

}  // namespace

DocumentMetadata::DocumentMetadata(RenderFrameHost* rfh) {
  rfh->GetRemoteInterfaces()->GetInterface(
      metadata_remote_.BindNewPipeAndPassReceiver());
}

DocumentMetadata::~DocumentMetadata() = default;

void DocumentMetadata::GetLoginActions(LoginActionsCallback callback) {
  metadata_remote_->GetEntities(base::BindOnce(&DocumentMetadata::OnGetEntities,
                                               base::Unretained(this),
                                               std::move(callback)));
}

void DocumentMetadata::OnGetEntities(LoginActionsCallback callback,
                                     blink::mojom::WebPagePtr page) {
  std::vector<blink::mojom::IdentityProviderGetParametersPtr> actions;
  if (page) {
    for (const auto& entity : page->entities) {
      auto params = Parse(*entity);
      if (params) {
        actions.push_back(std::move(*params));
      }
    }
  }
  std::move(callback).Run(std::move(actions));
}

}  // namespace content::webid
