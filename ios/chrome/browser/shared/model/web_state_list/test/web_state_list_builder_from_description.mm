// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"

#import <queue>
#import <ranges>
#import <sstream>

#import "base/strings/string_split.h"
#import "base/strings/string_util.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"

namespace {

// Different types of token in a description.
enum class TokenType {
  kInvalid,
  kWebStateIdentifier,
  kWebStateActivation,
  kPinnedWebStatesSeparator,
  kTabGroupOpeningBracket,
  kTabGroupIdentifier,
  kTabGroupClosingBracket,
  kMaxValue = kTabGroupClosingBracket,
};

// Returns the type of token associated with a character.
std::optional<TokenType> GetTokenTypeForCharacter(char character) {
  if (base::IsAsciiAlpha(character)) {
    return TokenType::kWebStateIdentifier;
  }
  if (base::IsAsciiDigit(character)) {
    return TokenType::kTabGroupIdentifier;
  }
  switch (character) {
    case ' ':
      return std::nullopt;
    case '|':
      return TokenType::kPinnedWebStatesSeparator;
    case '*':
      return TokenType::kWebStateActivation;
    case '[':
      return TokenType::kTabGroupOpeningBracket;
    case ']':
      return TokenType::kTabGroupClosingBracket;
    default:
      return TokenType::kInvalid;
  }
}

// A WebStateList description token.
struct Token {
  TokenType type;
  char character;
};

// Returns a list of tokens from `description`. Characters which cannot be
// associated with a token are simply ignored.
std::vector<Token> TokenizeWebStateListDescription(
    std::string_view description) {
  std::vector<Token> tokens;
  tokens.reserve(description.size());
  for (char character : description) {
    std::optional<TokenType> token_type = GetTokenTypeForCharacter(character);
    if (token_type) {
      tokens.push_back({.type = *token_type, .character = character});
    }
  }
  return tokens;
}

}  // namespace

WebStateListBuilderFromDescription::WebStateListBuilderFromDescription() =
    default;

WebStateListBuilderFromDescription::~WebStateListBuilderFromDescription() =
    default;

bool WebStateListBuilderFromDescription::BuildWebStateListFromDescription(
    WebStateList& web_state_list,
    std::string_view description) {
  if (!web_state_list.empty()) {
    return false;
  }

  bool parsing_pinned_web_states = true;
  char identifier_for_current_tab_group = 0;
  std::set<int> indices_for_current_tab_group;

  const std::vector<Token> tokens =
      TokenizeWebStateListDescription(description);
  for (int i = 0; i < static_cast<int>(tokens.size()); ++i) {
    const Token token = tokens[i];
    const std::optional<Token> prev_token =
        i - 1 >= 0 ? std::optional(tokens[i - 1]) : std::nullopt;
    const std::optional<Token> next_token =
        i + 1 < static_cast<int>(tokens.size()) ? std::optional(tokens[i + 1])
                                                : std::nullopt;
    switch (token.type) {
      case TokenType::kInvalid:
        return false;

      case TokenType::kWebStateIdentifier: {
        if (GetWebStateForIdentifier(token.character)) {
          return false;
        }
        auto web_state = std::make_unique<web::FakeWebState>();
        SetWebStateIdentifier(web_state.get(), token.character);
        web_state_list.InsertWebState(
            std::move(web_state),
            WebStateList::InsertionParams::AtIndex(web_state_list.count())
                .Pinned(parsing_pinned_web_states));
        if (identifier_for_current_tab_group) {
          indices_for_current_tab_group.insert(web_state_list.count() - 1);
        }
        break;
      }

      case TokenType::kWebStateActivation: {
        if (web_state_list.GetActiveWebState() != nullptr ||
            web_state_list.count() == 0 || !prev_token ||
            prev_token->type != TokenType::kWebStateIdentifier) {
          return false;
        }
        web_state_list.ActivateWebStateAt(web_state_list.count() - 1);
        break;
      }

      case TokenType::kPinnedWebStatesSeparator: {
        if (!parsing_pinned_web_states) {
          return false;
        }
        parsing_pinned_web_states = false;
        break;
      }

      case TokenType::kTabGroupOpeningBracket: {
        if (identifier_for_current_tab_group || parsing_pinned_web_states ||
            !next_token || next_token->type != TokenType::kTabGroupIdentifier ||
            GetTabGroupForIdentifier(next_token->character)) {
          return false;
        }
        identifier_for_current_tab_group = next_token->character;
        indices_for_current_tab_group.clear();
        break;
      }

      case TokenType::kTabGroupIdentifier: {
        break;
      }

      case TokenType::kTabGroupClosingBracket: {
        if (!identifier_for_current_tab_group ||
            indices_for_current_tab_group.empty()) {
          return false;
        }
        const TabGroup* created_group =
            web_state_list.CreateGroup(std::move(indices_for_current_tab_group),
                                       tab_groups::TabGroupVisualData());
        SetTabGroupIdentifier(created_group, identifier_for_current_tab_group);
        identifier_for_current_tab_group = 0;
        break;
      }
    }
  }

  if (parsing_pinned_web_states || identifier_for_current_tab_group) {
    return false;
  }

  return true;
}

std::string WebStateListBuilderFromDescription::GetWebStateListDescription(
    const WebStateList& web_state_list) const {
  if (web_state_list.empty()) {
    return "|";
  }

  std::ostringstream oss;
  bool pinned_web_states_separator_added = false;
  for (int i = 0; i < web_state_list.count(); ++i) {
    const WebState* web_state = web_state_list.GetWebStateAt(i);
    const TabGroup* tab_group = web_state_list.GetGroupOfWebStateAt(i);
    const WebState* prev_web_state = web_state_list.ContainsIndex(i - 1)
                                         ? web_state_list.GetWebStateAt(i - 1)
                                         : nullptr;
    const TabGroup* prev_tab_group =
        prev_web_state ? web_state_list.GetGroupOfWebStateAt(i - 1) : nullptr;
    const WebState* next_web_state = web_state_list.ContainsIndex(i + 1)
                                         ? web_state_list.GetWebStateAt(i + 1)
                                         : nullptr;
    const TabGroup* next_tab_group =
        next_web_state ? web_state_list.GetGroupOfWebStateAt(i + 1) : nullptr;

    if (!pinned_web_states_separator_added &&
        !web_state_list.IsWebStatePinnedAt(i) &&
        (!prev_web_state || web_state_list.IsWebStatePinnedAt(i - 1))) {
      pinned_web_states_separator_added = true;
      oss << "| ";
    }

    if (tab_group != prev_tab_group && tab_group != nullptr) {
      oss << "[ " << GetTabGroupIdentifier(tab_group) << " ";
    }
    oss << GetWebStateIdentifier(web_state)
        << ((web_state_list.active_index() == i) ? "* " : " ");
    if (tab_group != next_tab_group && tab_group != nullptr) {
      oss << "] ";
    }

    if (!pinned_web_states_separator_added &&
        web_state_list.IsWebStatePinnedAt(i) &&
        (!next_web_state || !web_state_list.IsWebStatePinnedAt(i + 1))) {
      pinned_web_states_separator_added = true;
      oss << "| ";
    }
  }

  std::string result = oss.str();
  if (!result.empty()) {
    // Remove trailing space if any.
    CHECK_EQ(' ', result.back());
    result.pop_back();
  }
  return result;
}

std::string WebStateListBuilderFromDescription::FormatWebStateListDescription(
    std::string_view description) const {
  std::string result = base::CollapseWhitespaceASCII(description, false);
  for (int i = result.size() - 2; i >= 0; --i) {
    // Inserting missing spaces.
    const char curr = result[i];
    const char next = result[i + 1];
    if (curr == ' ' || next == ' ' ||
        (base::IsAsciiAlpha(curr) && next == '*')) {
      continue;
    }
    result.insert(i + 1, " ");
  }
  return result;
}

const web::WebState*
WebStateListBuilderFromDescription::GetWebStateForIdentifier(
    char identifier) const {
  auto found_web_state_it = web_state_for_identifier_.find(identifier);
  return found_web_state_it != end(web_state_for_identifier_)
             ? found_web_state_it->second.get()
             : nullptr;
}

const TabGroup* WebStateListBuilderFromDescription::GetTabGroupForIdentifier(
    char identifier) const {
  auto found_tab_group_it = tab_group_for_identifier_.find(identifier);
  return found_tab_group_it != end(tab_group_for_identifier_)
             ? found_tab_group_it->second.get()
             : nullptr;
}

char WebStateListBuilderFromDescription::GetWebStateIdentifier(
    const WebState* web_state) const {
  auto found_identifier_it = identifier_for_web_state_.find(web_state);
  return found_identifier_it != end(identifier_for_web_state_)
             ? found_identifier_it->second
             : '_';
}

char WebStateListBuilderFromDescription::GetTabGroupIdentifier(
    const TabGroup* tab_group) const {
  auto found_identifier_it = identifier_for_tab_group_.find(tab_group);
  return found_identifier_it != end(identifier_for_tab_group_)
             ? found_identifier_it->second
             : '_';
}

void WebStateListBuilderFromDescription::SetWebStateIdentifier(
    const WebState* web_state,
    char new_identifier) {
  CHECK(web_state);
  CHECK(base::IsAsciiAlpha(new_identifier));
  CHECK(!GetWebStateForIdentifier(new_identifier));
  const char old_identifier = GetWebStateIdentifier(web_state);
  if (old_identifier != '_') {
    web_state_for_identifier_.erase(old_identifier);
  }
  web_state_for_identifier_[new_identifier] = web_state;
  identifier_for_web_state_[web_state] = new_identifier;
}

void WebStateListBuilderFromDescription::SetTabGroupIdentifier(
    const TabGroup* tab_group,
    char new_identifier) {
  CHECK(tab_group);
  CHECK(base::IsAsciiDigit(new_identifier));
  CHECK(!GetTabGroupForIdentifier(new_identifier));
  const char old_identifier = GetTabGroupIdentifier(tab_group);
  if (old_identifier != '_') {
    tab_group_for_identifier_.erase(old_identifier);
  }
  identifier_for_tab_group_[tab_group] = new_identifier;
  tab_group_for_identifier_[new_identifier] = tab_group;
}

void WebStateListBuilderFromDescription::GenerateIdentifiersForWebStateList(
    const WebStateList& web_state_list) {
  std::queue<char> available_web_state_identifiers;
  for (char identifier = 'a'; identifier <= 'z'; ++identifier) {
    if (!GetWebStateForIdentifier(identifier)) {
      available_web_state_identifiers.push(identifier);
    }
  }
  for (char identifier = 'A'; identifier <= 'Z'; ++identifier) {
    if (!GetWebStateForIdentifier(identifier)) {
      available_web_state_identifiers.push(identifier);
    }
  }

  std::queue<char> available_tab_group_identifiers;
  for (char identifier = '0'; identifier <= '9'; ++identifier) {
    if (!GetTabGroupForIdentifier(identifier)) {
      available_tab_group_identifiers.push(identifier);
    }
  }

  for (int i = 0; i < web_state_list.count(); ++i) {
    const WebState* web_state = web_state_list.GetWebStateAt(i);
    if (GetWebStateIdentifier(web_state) == '_') {
      CHECK(!available_web_state_identifiers.empty());
      char identifier = available_web_state_identifiers.front();
      available_web_state_identifiers.pop();
      SetWebStateIdentifier(web_state, identifier);
    }

    const TabGroup* tab_group = web_state_list.GetGroupOfWebStateAt(i);
    if (tab_group && GetTabGroupIdentifier(tab_group) == '_') {
      CHECK(!available_tab_group_identifiers.empty());
      char identifier = available_tab_group_identifiers.front();
      available_tab_group_identifiers.pop();
      SetTabGroupIdentifier(tab_group, identifier);
    }
  }
}
