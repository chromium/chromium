---
name: ios/define-subcomponent-types-in-subcomponent-header
description: When declaring shared enums, structs, or configuration types, place them in dedicated type, constants, or subcomponent headers rather than container, protocol, class, or view headers
rubric: design
severity: should_fix
---

**Rule.** Do not declare enums, configuration types, or data structs inside
parent container headers, caller implementation files, specific class headers,
or view headers. Place subcomponent-specific types in the subcomponent's own
header, co-locate intermediate structs in utility headers (`*_util.h`), and
define types shared across components, models, mediators, or services in
dedicated constants or type headers (e.g., `*_constants.h`,
`*_type.h`/`*_types.h`, or `*_item.h`). A type used only by one consumer,
mutator, or data-source relationship may be declared in that protocol's header
in `ui/` (for example, the `enum class` state that a consumer's `-set<State>:`
method takes). In headers where shared types or enums are only referenced in
signatures or properties, use forward declarations (such as `struct StructName;`
or `enum class EnumName : NSInteger;`) rather than importing the defining
header.

**Why.** Declaring shared types inside container, view, class, or caller
implementation files inverts dependency hierarchies, couples data definitions to
specific UI or class interfaces, and forces callers to pull in heavy
dependencies just for type definitions. Placing types in dedicated constants,
types, or utility headers preserves modular layering, while forward declarations
prevent unnecessary rebuilds across transitive consumers.

## Examples

Bad:

```cpp
// 1. Subcomponent enum declared in parent container header:
// In parent_container_view.h:
enum class TrailingIconType { kRefineQuery, kOpenExistingTab, kNone };

// In subcomponent_trailing_button.h (inverted dependency):
#import "parent_container_view.h"

// 2. Item struct shared with other components colocated in one consumer header:
// In assistant_aim_consumer.h:
struct AssistantAIMHistoryItem {
  std::string task_id;
  std::string title;
};

// 3. Intermediate struct declared in caller implementation file:
// In page_context_wrapper.mm:
struct FrameFocusInfo {
  std::string document_id;
  std::optional<autofill::LocalFrameToken> local_token;
};

// 4. Shared enum declared in specific view or class header:
// In bubble_view.h:
enum class BubblePageControlPage { kNone, kFirst };
// In gemini_settings_action.h (couples consumers to class and UIKit):
enum class GeminiSettingsActionType : NSInteger { kUnknown, kURL };
@interface GeminiSettingsAction : NSObject
@property(nonatomic, readonly) GeminiSettingsActionType type;
@end
```

Good:

```cpp
// 1. Enum declared in subcomponent header (parent depends on child):
// In subcomponent_trailing_button.h:
enum class TrailingIconType { kRefineQuery, kOpenExistingTab, kNone };

// 2. Item struct extracted into a dedicated header:
// In assistant_aim_history_item.h:
struct AssistantAIMHistoryItem {
  std::string task_id;
  std::string title;
};

// 3. Intermediate struct co-located in utility header alongside helpers:
// In annotated_page_content_extraction_utils.h:
struct FrameFocusInfo {
  std::string document_id;
  std::optional<autofill::LocalFrameToken> local_token;
};
void ResolveFocusedFrame(std::vector<FrameFocusInfo>& frame_infos, ...);

// 4. Shared enums in dedicated constants/type headers; forward-declared in referencing headers:
// In bubble_constants.h:
enum class BubblePageControlPage { kNone, kFirst };
// In gemini_settings_action_type.h:
enum class GeminiSettingsActionType : NSInteger { kUnknown, kURL };
// In gemini_settings_action.h:
enum class GeminiSettingsActionType : NSInteger;
@interface GeminiSettingsAction : NSObject
@property(nonatomic, readonly) GeminiSettingsActionType type;
@end
```
