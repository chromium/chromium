// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fuzztest_utils/dom_scenario.h"

#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/types/optional_ref.h"
#include "base/types/optional_util.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

namespace {

std::string AttributesToString(
    const std::vector<std::pair<QualifiedName, std::string>>* attributes) {
  if (!attributes || attributes->empty()) {
    return std::string("None");
  }
  std::string out;
  for (const auto& attr : *attributes) {
    base::StrAppend(
        &out, {" ", attr.first.ToString().Utf8(), "='", attr.second, "'"});
  }
  return out;
}

std::string FormatOptionalString(
    base::optional_ref<const std::string> opt_str) {
  return opt_str.has_value() ? base::StrCat({"'", *opt_str, "'"}) : "unset";
}

std::string DomScenarioToString(const DomScenario& scenario) {
  std::string out;
  base::StrAppend(&out, {"\n---------- DomScenario ----------\n"});
  base::StrAppend(
      &out, {"Root Element: ", scenario.root_tag.ToString().Utf8(), "\n"});

  base::StrAppend(
      &out, {"Nodes (", base::ToString(scenario.node_specs.size()), "):\n"});
  for (size_t i = 0; i < scenario.node_specs.size(); ++i) {
    const auto& spec = scenario.node_specs[i];
    const auto& initial = spec.initial_state;
    const auto& modified = spec.modified_state;

    base::StrAppend(&out, {base::ToString(i), ". ", spec.tag.ToString().Utf8(),
                           " (id=id_", base::ToString(i), ")\n"});

    if (initial.parent_index != modified.parent_index) {
      base::StrAppend(
          &out, {"   Parent Index: ", base::ToString(initial.parent_index),
                 " -> ", base::ToString(modified.parent_index), "\n"});
    } else {
      base::StrAppend(
          &out, {"   Parent Index: ", base::ToString(initial.parent_index),
                 " (Unchanged)\n"});
    }

    if (initial.text != modified.text) {
      base::StrAppend(&out, {"   Text: "});
      if (!initial.text.has_value()) {
        base::StrAppend(&out,
                        {"not set -> ", FormatOptionalString(modified.text)});
      } else {
        base::StrAppend(&out, {FormatOptionalString(initial.text), " -> ",
                               FormatOptionalString(modified.text)});
      }
      base::StrAppend(&out, {"\n"});
    } else if (initial.text.has_value()) {
      base::StrAppend(&out, {"   Text: ", FormatOptionalString(initial.text),
                             " (Unchanged)\n"});
    }

    if (initial.styles != modified.styles) {
      base::StrAppend(&out, {"   Styles: "});
      if (!initial.styles.has_value()) {
        base::StrAppend(&out,
                        {"not set -> ", FormatOptionalString(modified.styles)});
      } else {
        base::StrAppend(&out, {FormatOptionalString(initial.styles), " -> ",
                               FormatOptionalString(modified.styles)});
      }
      base::StrAppend(&out, {"\n"});
    } else if (initial.styles.has_value()) {
      base::StrAppend(&out,
                      {"   Styles: ", FormatOptionalString(initial.styles),
                       " (Unchanged)\n"});
    }

    std::string initial_attrs_str =
        AttributesToString(base::OptionalToPtr(initial.attributes));
    std::string modified_attrs_str =
        AttributesToString(base::OptionalToPtr(modified.attributes));
    if (initial_attrs_str != modified_attrs_str) {
      base::StrAppend(&out, {"   Attributes:"});
      if (initial_attrs_str == "None") {
        base::StrAppend(&out, {" not set ->", modified_attrs_str});
      } else if (modified_attrs_str == "None") {
        base::StrAppend(&out, {initial_attrs_str, " -> unset"});
      } else {
        base::StrAppend(&out, {initial_attrs_str, " ->", modified_attrs_str});
      }
      base::StrAppend(&out, {"\n"});
    } else if (initial_attrs_str != "None") {
      base::StrAppend(&out,
                      {"   Attributes:", initial_attrs_str, " (Unchanged)\n"});
    }
  }
  base::StrAppend(&out, {"--------------------------------------"});
  return out;
}

}  // namespace

// Domain for a node's state (parent index, attributes, styles, text).
fuzztest::Domain<NodeState> AnyNodeState(DomScenarioDomainSpecification* spec,
                                         int num_nodes) {
  return fuzztest::StructOf<NodeState>(
      fuzztest::InRange(kIndexOfRootElement, num_nodes - 1),
      fuzztest::OptionalOf(fuzztest::VectorOf(spec->AnyAttributeNameValuePair())
                               .WithMaxSize(spec->GetMaxAttributesPerNode())),
      fuzztest::OptionalOf(spec->AnyStyles()),
      fuzztest::OptionalOf(spec->AnyText()));
}

// Domain for a complete node specification (tag + initial state + modified
// state).
fuzztest::Domain<NodeSpecification> AnyNodeSpecification(
    DomScenarioDomainSpecification* spec,
    int num_nodes) {
  return fuzztest::StructOf<NodeSpecification>(spec->AnyTag(),
                                               AnyNodeState(spec, num_nodes),
                                               AnyNodeState(spec, num_nodes));
}

fuzztest::Domain<DomScenario> AnyDomScenarioForSpec(
    DomScenarioDomainSpecification* spec) {
  return fuzztest::FlatMap(
      [spec](int num_nodes) {
        return fuzztest::StructOf<DomScenario>(
            spec->GetRootElementTag(),
            fuzztest::VectorOf(AnyNodeSpecification(spec, num_nodes))
                .WithSize(num_nodes));
      },
      fuzztest::InRange(1, spec->GetMaxDomNodes()));
}

std::string DomScenario::ToString() const {
  return DomScenarioToString(*this);
}

}  // namespace blink
