/*
 * Copyright (C) 2020 Google Inc. All rights reserved.
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

#include "third_party/blink/public/common/security/address_space_feature.h"

#include <iosfwd>
#include <string>
#include <vector>

#include "services/network/public/cpp/ip_address_space_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

using AddressSpace = network::mojom::IPAddressSpace;
using Feature = mojom::WebFeature;

constexpr FetchType kAllFetchTypes[]{
    FetchType::kSubresource,
    FetchType::kNavigation,
};

std::string FetchTypeToString(FetchType type) {
  switch (type) {
    case FetchType::kSubresource:
      return "FetchType::kSubresource";
    case FetchType::kNavigation:
      return "FetchType::kNavigation";
  }
}

std::ostream& operator<<(std::ostream& out, FetchType type) {
  return out << FetchTypeToString(type);
}

constexpr AddressSpace kAllAddressSpaces[] = {
    AddressSpace::kUnknown,
    AddressSpace::kPublic,
    AddressSpace::kPrivate,
    AddressSpace::kLocal,
};

// Encapsulates arguments to AddressSpaceFeature.
struct Input {
  FetchType fetch_type;
  AddressSpace client_address_space;
  bool client_is_secure_context;
  AddressSpace resource_address_space;
};

// Convenience for HasMappedFeature().
bool operator==(const Input& lhs, const Input& rhs) {
  return lhs.fetch_type == rhs.fetch_type &&
         lhs.client_address_space == rhs.client_address_space &&
         lhs.client_is_secure_context == rhs.client_is_secure_context &&
         lhs.resource_address_space == rhs.resource_address_space;
}

// Allows use of Input arguments to SCOPED_TRACE().
std::ostream& operator<<(std::ostream& out, const Input& input) {
  return out << "Input{ fetch_type: " << input.fetch_type
             << ", client_address_space: " << input.client_address_space
             << ", client_is_secure_context: " << input.client_is_secure_context
             << ", resource_address_space: " << input.resource_address_space
             << " }";
}

// Returns all possible Input values.
std::vector<Input> AllInputs() {
  std::vector<Input> result;

  for (FetchType fetch_type : kAllFetchTypes) {
    for (AddressSpace client_address_space : kAllAddressSpaces) {
      for (bool client_is_secure_context : {false, true}) {
        for (AddressSpace resource_address_space : kAllAddressSpaces) {
          result.push_back({
              fetch_type,
              client_address_space,
              client_is_secure_context,
              resource_address_space,
          });
        }
      }
    }
  }
  return result;
}

// Convenience: calls AddressSpaceFeatureForSubresource() on input's components.
std::optional<Feature> AddressSpaceFeatureForInput(const Input& input) {
  return AddressSpaceFeature(input.fetch_type, input.client_address_space,
                             input.client_is_secure_context,
                             input.resource_address_space);
}

// Maps an input to an expected Feature value.
struct FeatureMapping {
  Input input;
  Feature feature;
};

// The list of all features and their mapped inputs.
constexpr FeatureMapping kFeatureMappings[] = {
    {
        {FetchType::kSubresource, AddressSpace::kUnknown, false,
         AddressSpace::kPrivate},
        Feature::kAddressSpaceUnknownNonSecureContextEmbeddedPrivate,
    },
    {
        {FetchType::kSubresource, AddressSpace::kUnknown, true,
         AddressSpace::kPrivate},
        Feature::kAddressSpaceUnknownSecureContextEmbeddedPrivate,
    },
    {
        {FetchType::kSubresource, AddressSpace::kUnknown, false,
         AddressSpace::kLocal},
        Feature::kAddressSpaceUnknownNonSecureContextEmbeddedLocal,
    },
    {
        {FetchType::kSubresource, AddressSpace::kUnknown, true,
         AddressSpace::kLocal},
        Feature::kAddressSpaceUnknownSecureContextEmbeddedLocal,
    },
    {
        {FetchType::kSubresource, AddressSpace::kPublic, false,
         AddressSpace::kPrivate},
        Feature::kAddressSpacePublicNonSecureContextEmbeddedPrivate,
    },
    {
        {FetchType::kSubresource, AddressSpace::kPublic, true,
         AddressSpace::kPrivate},
        Feature::kAddressSpacePublicSecureContextEmbeddedPrivate,
    },
    {
        {FetchType::kSubresource, AddressSpace::kPublic, false,
         AddressSpace::kLocal},
        Feature::kAddressSpacePublicNonSecureContextEmbeddedLocal,
    },
    {
        {FetchType::kSubresource, AddressSpace::kPublic, true,
         AddressSpace::kLocal},
        Feature::kAddressSpacePublicSecureContextEmbeddedLocal,
    },
    {
        {FetchType::kSubresource, AddressSpace::kPrivate, false,
         AddressSpace::kLocal},
        Feature::kAddressSpacePrivateNonSecureContextEmbeddedLocal,
    },
    {
        {FetchType::kSubresource, AddressSpace::kPrivate, true,
         AddressSpace::kLocal},
        Feature::kAddressSpacePrivateSecureContextEmbeddedLocal,
    },
    {
        {FetchType::kNavigation, AddressSpace::kUnknown, false,
         AddressSpace::kPrivate},
        Feature::kAddressSpaceUnknownNonSecureContextNavigatedToPrivate,
    },
    {
        {FetchType::kNavigation, AddressSpace::kUnknown, true,
         AddressSpace::kPrivate},
        Feature::kAddressSpaceUnknownSecureContextNavigatedToPrivate,
    },
    {
        {FetchType::kNavigation, AddressSpace::kUnknown, false,
         AddressSpace::kLocal},
        Feature::kAddressSpaceUnknownNonSecureContextNavigatedToLocal,
    },
    {
        {FetchType::kNavigation, AddressSpace::kUnknown, true,
         AddressSpace::kLocal},
        Feature::kAddressSpaceUnknownSecureContextNavigatedToLocal,
    },
    {
        {FetchType::kNavigation, AddressSpace::kPublic, false,
         AddressSpace::kPrivate},
        Feature::kAddressSpacePublicNonSecureContextNavigatedToPrivate,
    },
    {
        {FetchType::kNavigation, AddressSpace::kPublic, true,
         AddressSpace::kPrivate},
        Feature::kAddressSpacePublicSecureContextNavigatedToPrivate,
    },
    {
        {FetchType::kNavigation, AddressSpace::kPublic, false,
         AddressSpace::kLocal},
        Feature::kAddressSpacePublicNonSecureContextNavigatedToLocal,
    },
    {
        {FetchType::kNavigation, AddressSpace::kPublic, true,
         AddressSpace::kLocal},
        Feature::kAddressSpacePublicSecureContextNavigatedToLocal,
    },
    {
        {FetchType::kNavigation, AddressSpace::kPrivate, false,
         AddressSpace::kLocal},
        Feature::kAddressSpacePrivateNonSecureContextNavigatedToLocal,
    },
    {
        {FetchType::kNavigation, AddressSpace::kPrivate, true,
         AddressSpace::kLocal},
        Feature::kAddressSpacePrivateSecureContextNavigatedToLocal,
    },
};

// Returns true if input is mapped to a feature in kFeatureMappings.
bool HasMappedFeature(const Input& input) {
  for (const FeatureMapping& mapping : kFeatureMappings) {
    if (input == mapping.input) {
      return true;
    }
  }
  return false;
}

// This test verifies that AddressSpaceFeature stays in sync with the reference
// implementation for Private Network Access address space checks in
// services/networ. In more practical terms, it verifies that
// `AddressSpaceFeature()` returns a feature (as opposed to `nullopt`) if and
// only if the resource address space is less public than the client address
// space.
TEST(AddressSpaceFeatureTest, ReturnsFeatureIffResourceLessPublic) {
  for (const Input& input : AllInputs()) {
    SCOPED_TRACE(input);

    auto optional_feature = AddressSpaceFeatureForInput(input);

    bool should_have_feature = network::IsLessPublicAddressSpace(
        input.resource_address_space, input.client_address_space);

    if (should_have_feature) {
      EXPECT_TRUE(optional_feature.has_value());
    } else {
      EXPECT_FALSE(optional_feature.has_value()) << *optional_feature;
    }
  }
}

// This test verifies that `AddressSpaceFeature()` maps inputs to features as
// declared in `kFeatureMappings`.
TEST(AddressSpaceFeatureTest, MapsAllFeaturesCorrectly) {
  for (const FeatureMapping& mapping : kFeatureMappings) {
    SCOPED_TRACE(mapping.input);

    auto optional_feature = AddressSpaceFeatureForInput(mapping.input);

    ASSERT_TRUE(optional_feature.has_value());
    EXPECT_EQ(mapping.feature, *optional_feature);
  }
}

// This test verifies that all inputs that yield a Feature when run through
// `AddressSpaceFeature()` are included in `kFeatureMappings`.
TEST(AddressSpaceFeatureTest, FeatureMappingsAreComplete) {
  for (const Input& input : AllInputs()) {
    SCOPED_TRACE(input);

    auto optional_feature = AddressSpaceFeatureForInput(input);

    if (HasMappedFeature(input)) {
      EXPECT_TRUE(optional_feature.has_value());
    } else {
      EXPECT_FALSE(optional_feature.has_value()) << *optional_feature;
    }
  }
}

}  // namespace
}  // namespace blink
