# Site Search in Omnibox Explainer

This document provides a technical overview of the Omnibox Scoped Search (also referred to as "site
search" or "keyword search") functionality for Chrome engineers. The architecture and code paths
described are shared across Windows, Mac, Linux, and ChromeOS. It is based on a detailed code
analysis as of this writing (2025-09-24) covers two primary scenarios: scoped search via traditional
keywords (e.g. youtube.com) and scoped search via built-in keywords (also known as "starter pack"
keywords), which use the "@" syntax (e.g., @history).

## Types of Keywords

Keywords in the Omnibox come from various sources and have different behaviors. Here's a breakdown
of the most common types:

- **Starter Packs**: Built-in keywords with the "@" prefix (e.g., `@bookmarks`, `@history`).
- **Prepopulated**: Keywords for common search engines (e.g., `bing.com`, `yahoo.com`).
- **User Created (Traditional)**: A user-created keyword where the template URL contains a %s
  placeholder (e.g., `youtube` search).
- **Non-substituting User Created**: A user-created keyword where the template URL does not contain
  a %s placeholder.
- **Default Search Engine (DSE)**: The user's currently selected default search engine, which is
  kind of orthogonal to the other types here.
- **Extension**: Keywords registered by browser extensions (e.g., an `ssh` keyword registered by the
  Secure Shell extension).
- **Enterprise Policy**: Keywords set by a device administrator through enterprise policies. These
  can be further subdivided into:
  - **Aggregator**: e.g.,`@agentspace`.
  - **Non-aggregator**: e.g., `microsoft-documents`.
- **Admin Policy**: Keywords set by an admin policy.

## High-Level Overview

The Omnibox uses a sophisticated system of providers, managed by the
[`AutocompleteController`](https://source.chromium.org/chromium/chromium/src/+/main:components/omnibox/browser/autocomplete_controller.h),
to generate suggestions. Site Search functionality is primarily handled by two distinct providers:

1. **[`FeaturedSearchProvider`](https://source.chromium.org/chromium/chromium/src/+/main:components/omnibox/browser/featured_search_provider.h)**:
   Manages the "Omnibox Starter Pack," which provides scoped search suggestions like `@history`,
   `@bookmarks`, and `@tabs`, as well as enterprise keywords (e.g. @agentspace or
   facebook-internal-documents).
2. **[`KeywordProvider`](https://source.chromium.org/chromium/chromium/src/+/main:components/omnibox/browser/keyword_provider.h)**:
   Manages searches directed at specific, non-default search engines using keywords. This is powered
   by the
   [`TemplateURLService`](https://source.chromium.org/chromium/chromium/src/+/main:components/search_engines/template_url_service.h),
   which stores URL templates for various sites.

While `KeywordProvider` and `FeaturedSearchProvider` manage surfacing the keyword entry points, most
providers (e.g., tabs, bookmarks, history, search, ...) are responsible for populating the
suggestions when in keyword mode. e.g.

- 'youtube.com query<enter>' will be handled by search provider.
- 'youtube.com cat video<down><down><enter>' to select a past youtube navigation will be handled by
  a history provider.

These providers rely on the `TemplateURLService`, which is populated with
[`TemplateURL`](https://source.chromium.org/chromium/chromium/src/+/main:components/search_engines/template_url.h)
objects. These objects can come from several sources:

- For enterprise and policy keywords, they're set by policy.
- For pre-populated keywords, they're hardcoded into the binary, but in a json or some sort of data
  file, not necessarily in .cc directly.
- For extensions, they come from the extensions.

The general flow is as follows:

1. User types into the Omnibox.
2. `AutocompleteController` starts and queries all registered providers with the user's input.
3. `FeaturedSearchProvider` and `KeywordProvider`, among others, generate
   [`AutocompleteMatch`](https://source.chromium.org/chromium/chromium/src/+/main:components/omnibox/browser/autocomplete_match.h)
   objects based on the input.
4. These matches are ranked and displayed to the user.
5. When a user selects a match that happens to have a keyword chip, they will navigate to the match,
   ignoring the keyword chip. When a user selects the keyword chip on the match, they will enter
   keyword mode (to handle subsequent input) which may or may not ultimately navigate to the target
   site. (You can enter youtube keyword mode by typing 'youtube.com query' then select a
   drive.google.com document titled 'youtube.com query' that does not navigate to youtube.com.)

## Key UI Component Roles

The `LocationBarView` is the top level component of omnibox view hierarchy. See
[Overview of the OmniboxView Hierarchy](https://docs.google.com/document/d/1CPAEWojYkGxRL6j3REnWtVFbDz8A7vtFXf9RXmmWyEY)
(internal document) for a picture.

- **[`LocationBarView`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/location_bar/location_bar_view.h)**
  owns the omnibox view, the popup view, and selected keyword view. It is mostly responsible for
  updating the keyword chip in the input row (e.g., "Search History").

- **[`OmniboxView`](https://source.chromium.org/chromium/chromium/src/+/main:components/omnibox/browser/omnibox_view.h)**:
  This is the text field where the user types. Its implementation (e.g., `OmniboxViewViews`) is
  responsible for rendering the user's text, and handling input. It communicates user actions, like
  typing, to the `OmniboxEditModel`.

- **[`OmniboxPopupView`](https://source.chromium.org/chromium/chromium/src/+/main:components/omnibox/browser/omnibox_popup_view.h)**:
  This is the dropdown list of suggestions that appears below the text field. Its implementation
  (e.g., `OmniboxPopupViewViews`) is responsible for rendering each suggestion line (using
  `OmniboxResultView`). It receives the list of `AutocompleteMatch` objects from the model and
  informs the model when the user interacts with the list (e.g., hovering or selecting a
  suggestion).

The `OmniboxEditModel` acts as a controller for these views. For example, when a user selects a
suggestion in the `OmniboxPopupView`, the popup notifies the model. The model then updates its
internal state and may, in turn, commands the `OmniboxView` to change its displayed text or show a
keyword chip.

While the view and model are mutually aware of each other to facilitate this communication, their
ownership is hierarchical to prevent circular dependencies. The `OmniboxView` is owned by its parent
UI component (e.g., `LocationBarView`). The view, in turn, owns the
[`OmniboxController`](https://source.chromium.org/chromium/chromium/src/+/main:components/omnibox/browser/omnibox_controller.h),
which owns the `OmniboxEditModel`. This creates a clear ownership chain: `OmniboxView` →
`OmniboxController` → `OmniboxEditModel`. Communication back up the chain is handled via non-owning
`raw_ptr` references.

## Scenario 1: Scoped Search (`@history`)

This scenario describes how a user can scope their search to their local browser history.

**User Journey:**

1. User types `@hist`.
2. An autocomplete suggestion for "Search History" appears.
3. User selects this suggestion.
4. The text `@history` disappears from the input field and is replaced by a "Search History" chip.
5. User types a query, e.g., `foo`, and presses Enter.
6. The browser navigates to `chrome://history/?q=foo`.

### Technical Deep Dive

1. **Suggestion Generation (`@hist`)**

   - **Component**: `FeaturedSearchProvider`, `TemplateURLService`
   - **Mechanism**: The `@` prefixed suggestions are part of the "Omnibox Starter Pack" feature,
     which are managed as special `TemplateURL` objects.
     - The `TemplateURLService` is loaded with a set of default "starter pack" engines, including
       one for History with the keyword `@history` and a unique `starter_pack_id`. This data is
       loaded from `components/search_engines/template_url_starter_pack_data.cc`.
     - `FeaturedSearchProvider::Start()` is called on input. It detects that the input starts with
       `@` and queries the `TemplateURLService` for matching keywords.
     - It finds the `@history` `TemplateURL` object.
     - `FeaturedSearchProvider` then creates an `AutocompleteMatch` with the type
       `AutocompleteMatch::Type::STARTER_PACK`.

2. **Mode Activation (Selecting the suggestion)**

   - **Component**:
     [`OmniboxEditModel`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/omnibox/omnibox_edit_model.h),
     `AutocompleteController`
   - **Mechanism**: When the user selects the `STARTER_PACK` match, `OmniboxEditModel::OpenMatch()`
     is called. This activates "keyword mode" with `@history` as the keyword.
     - The `AutocompleteController` is restarted with the new input. It recognizes that the omnibox
       is in keyword mode with a `TemplateURL` that has a `starter_pack_id` for history.
     - Crucially, the controller then adjusts its logic to only run a limited set of providers
       relevant to history search:
       [`HistoryURLProvider`](https://source.chromium.org/chromium/chromium/src/+/main:components/omnibox/browser/history_url_provider.h),
       [`HistoryEmbeddingsProvider`](https://source.chromium.org/chromium/chromium/src/+/main:components/omnibox/browser/history_embeddings_provider.h),
       and `FeaturedSearchProvider`.

3. **UI State Management (The Chip and Text Replacement)**

   - **Component**: `OmniboxEditModel`,
     [`OmniboxView`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/omnibox/omnibox_view.h)
   - **Mechanism**: The separation between the model's state and the view's displayed text is key.
     - The `OmniboxEditModel` sets its internal state to keyword mode, storing `@history`
       internally.
     - It then clears the user's visible text, replacing it with an empty string.
     - The model notifies the `OmniboxView`, which sees that the model is in keyword mode
       (`is_keyword_selected()` is true) and renders the "Search History" chip instead of the
       keyword text.
     - Specifically, the
       [`SelectedKeywordView`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/location_bar/selected_keyword_view.h)
       is shown. It retrieves the `TemplateURL` for `@history`, gets its short name ("History"), and
       uses this to format a localized string (e.g., "Search History").

4. **Final Navigation (`foo` + Enter)**
   - **Component**: `HistoryURLProvider`, `AutocompleteController`
   - **Mechanism**: As the user types `foo`, the `AutocompleteController` runs the filtered set of
     providers (e.g., `HistoryURLProvider`) against the input `foo`.
     - `HistoryURLProvider` finds matching history entries for "foo".
     - It creates `AutocompleteMatch` objects with `destination_url`s pointing to the relevant
       history pages (e.g., `chrome://history/?q=foo`).
     - When the user presses Enter, the browser navigates to the selected history result.

## Scenario 2: Keyword Search (`youtube.com`)

This scenario describes how a user can search a specific website directly from the Omnibox using a
keyword.

**User Journey:**

1. User performs a search on `youtube.com`.
2. Chrome automatically generates an inactive `TemplateURL` for YouTube search by detecting the
   site's OpenSearch description.
3. The user navigates to `chrome://settings/searchEngines`.
4. The YouTube entry appears in the "inactive" list of site search engines.
5. The user explicitly activates the YouTube entry.
6. Now, when the user types "you" in the Omnibox, a suggestion for `youtube.com` appears with a
   "Search YouTube" hint, and site search is fully enabled.

### Technical Deep Dive

1. **Search Engine Discovery and Storage**

   - **Component**: `TemplateURLService`
   - **Mechanism**: When a user submits a search form on a website that provides an
     [OpenSearch description document](<https://en.wikipedia.org/wiki/OpenSearch_(specification)>),
     Chrome's renderer process can detect it and send an IPC message to the browser process. The
     `TemplateURLService` then creates and stores a `TemplateURL` object. This object contains the
     site's name, the keyword (e.g., `youtube.com`), and the URL template for performing a search
     (e.g., `https://www.youtube.com/results?search_query={searchTerms}`).

2. **Automatic Keyword Generation**

   - **Component**:
     [`SearchEngineTabHelper`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/search_engines/search_engine_tab_helper.h),
     `TemplateURLService`
   - **Mechanism**: When a user performs a search on a website, Chrome can automatically generate a
     keyword for that site.
     - `SearchEngineTabHelper::GenerateKeywordIfNecessary()` is called on navigation.
     - It checks if the navigation is a form submission and if a keyword can be generated from the
       URL.
     - If so, it calls `TemplateURLService::CanAddAutogeneratedKeyword()` to check if a new keyword
       can be added. This is to avoid adding duplicate or conflicting keywords.
     - If a new keyword can be added, a
       [`TemplateURLData`](https://source.chromium.org/chromium/chromium/src/+/main:components/search_engines/template_url_data.h)
       object is created and added to the `TemplateURLService`.
     - This newly created `TemplateURL` is marked as `safe_for_autoreplace`, meaning it can be
       replaced by a more official keyword later (e.g., one from an OpenSearch description
       document).
     - The `is_active` status is set to `kUnspecified`, so the keyword is not immediately usable.

3. **Activation**

   - **Component**: `TemplateURLService`
   - **Mechanism**: A newly created `TemplateURL` is not immediately active (`is_active` is
     `kUnspecified`). For it to be used for keyword search, it must be activated. There are two
     paths to activation:
     - **Manual Activation**: This is the immediate path. When a user goes to
       `chrome://settings/searchEngines` and activates an entry,
       `TemplateURLService::SetIsActiveTemplateURL()` is called. This function explicitly sets
       `is_active` to `kTrue` and also sets `safe_for_autoreplace` to `false`, ensuring the user's
       choice is preserved. This change takes effect immediately in the current session.
     - **Automatic Generation**: As described in the previous section, keywords can be generated
       automatically. However, they must be manually activated by the user in
       `chrome://settings/searchEngines`.

4. **Suggestion Generation (`youtube.com foo`)**

   - **Component**: `KeywordProvider`
   - **Mechanism**: `KeywordProvider::Start()` is called with the user's input.
     - It calls `AutocompleteInput::ExtractKeywordFromInput()` to parse the input into a potential
       keyword (`youtube.com`) and the remaining query (`foo`).
     - It then calls `TemplateURLService::AddMatchingKeywords()` with the extracted keyword.
     - `TemplateURLService` finds the active `TemplateURL` object matching `youtube.com`.
     - `KeywordProvider` receives this `TemplateURL` and proceeds to call its internal
       `CreateAutocompleteMatch()` method. This creates an `AutocompleteMatch` of type
       `AutocompleteMatch::Type::SEARCH_KEYWORD`.

5. **Final Navigation (Enter)**
   - **Component**: `KeywordProvider`
   - **Mechanism**: The final URL is constructed within `KeywordProvider::FillInUrlAndContents()`.
     - This function is called by `CreateAutocompleteMatch()`.
     - It uses the `TemplateURLRef` from the `TemplateURL` object. The `ReplaceSearchTerms()` method
       is called on this reference.
     - This method substitutes the `{searchTerms}` placeholder in the URL template with the user's
       query (`foo`).
     - The resulting URL (e.g., `https://www.youtube.com/results?search_query=foo`) is set as the
       `destination_url` of the `AutocompleteMatch`. When the user presses Enter, the browser
       navigates to this URL.

## Keyword State in Core Data Structures

To fully understand the logic, it's crucial to see how keyword state is tracked within the core data
structures: `AutocompleteInput` and `AutocompleteMatch`.

### `AutocompleteInput`

The `AutocompleteInput` object represents the user's query and the omnibox's state. It has two key
fields, `prefer_keyword_` and `allow_exact_keyword_match_`, which are used to signal that when the
user is in keyword mode, providers should show more "keyword-y" suggestions. For example, a search
for 'query' should suggest 'youtube.com/q=query' instead of 'google.com/q=query' when the user is in
the YouTube keyword mode.

### `AutocompleteMatch`

The `AutocompleteMatch` object represents a single suggestion and contains several nuanced fields
for managing keywords:

- **`associated_keyword`**: This field determines which keyword to activate via the keyword chip.
  For example, the `@history` keyword activates the `chrome://history`... template URL. This can be
  a source of confusion, because when a user types "youtube", an autocomplete match might appear
  that navigates to 'amazon.com/youtube' but also has a keyword chip to activate the `youtube.com`
  keyword. In this case, `associated_keyword` will store 'youtube.com', even though the rest of the
  match fields are Amazon-related.

- **`keyword`**: This field tracks which `TemplateURL` was used to create the match's destination
  URL. For example, if the user types 'youtube.com query', the `keyword` field will track 'youtube'.
  This can also be confusing, because even when the user isn't in keyword mode, most search
  suggestions are still generated using a keyword and will have their `keyword` field set (e.g., to
  `google.com` for the default search engine).

- **`from_keyword`**: This field tracks if the match was generated while the user is in keyword
  mode. This is another potential point of confusion, as it can be `true` even if the match doesn't
  have a `keyword` field set. Conversely, `from_keyword` can be `false` even when `keyword` is set.
  In essence, `from_keyword` reflects the _input state_ when the match was created, while `keyword`
  reflects the match's _data source_.

## Methods for Model-View Interaction

A key aspect of the Omnibox architecture is the interaction between the `OmniboxEditModel` (the
model) and its two main views. The model holds the state, while the views are responsible for
rendering the UI. The views are updated via a tightly-coupled observer-like pattern, where the model
directly calls methods on the views when its state changes.

The following table summarizes the key methods that facilitate this interaction within the scope of
the scenarios described in this document.

| Initiator (Caller)       | Target (Callee)    | Method Called                         | Purpose                                                                          |
| :----------------------- | :----------------- | :------------------------------------ | :------------------------------------------------------------------------------- |
| `OmniboxView`            | `OmniboxEditModel` | `OnAfterPossibleChange()`             | Notifies the model of user text input.                                           |
| `OmniboxEditModel`       | `OmniboxView`      | `SetWindowTextAndCaretPos()`          | Instructs the text view to change its content, e.g., when entering keyword mode. |
| `OmniboxPopupView`       | `OmniboxEditModel` | `OpenMatch()` / `SetPopupSelection()` | Notifies the model that the user has selected or highlighted a suggestion.       |
| `OmniboxEditModel`       | `OmniboxPopupView` | `UpdatePopupAppearance()`             | Instructs the popup to redraw itself based on the model's current state.         |
| `AutocompleteController` | `OmniboxEditModel` | `OnResultChanged()`                   | Notifies the model that the autocomplete results have changed.                   |

---

## Appendix: Architecture on Chrome for Android

The architecture described above is specific to desktop platforms which use the C++ Views UI
framework. Chrome for Android has a distinct architecture where the UI is primarily implemented in
Java and built upon a `Coordinator` pattern. This pattern distributes responsibilities among several
specialized components, in contrast to the more monolithic `OmniboxEditModel` on desktop.

However, the core suggestion generation logic is shared. The C++ `AutocompleteController` is used by
all platforms to generate suggestion results.

### The Coordinator Pattern on Android

On Android, the Omnibox (known as the Location Bar) is managed by a top-level
[`LocationBarCoordinator`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/LocationBarCoordinator.java).
This coordinator owns and orchestrates a set of sub-components, each with a specific responsibility.
The responsibilities of the desktop `OmniboxEditModel` are split among these components:

- **`LocationBarCoordinator`**: The overall owner and assembler. It creates the other coordinators
  and mediators and wires them together.

- **[`AutocompleteCoordinator`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/suggestions/AutocompleteCoordinator.java)**:
  Manages the suggestion list UI, including the
  [`OmniboxSuggestionsDropdown`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/suggestions/OmniboxSuggestionsDropdown.java)
  view. It creates and owns the `AutocompleteMediator`.

- **[`AutocompleteMediator`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/suggestions/AutocompleteMediator.java)**:
  The primary logic hub for autocomplete. It is responsible for communicating with the C++
  `AutocompleteController` via JNI, processing the results, and updating the `PropertyModel` that
  drives the suggestion list UI.

- **[`UrlBarCoordinator`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/UrlBarCoordinator.java)**:
  Manages the
  [`UrlBar`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/UrlBar.java)
  view, which is the `EditText` field the user types into. It handles text state, cursor position,
  and selection, reporting user input to other components.

- **[`LocationBarMediator`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/LocationBarMediator.java)**:
  A central logic hub that connects the other components. It handles events like focus changes and
  button clicks, and manages the overall state of the location bar that doesn't directly involve
  text editing or autocomplete suggestions.

### Interaction Flow for Suggestions

1. A user types into the `UrlBar` view (managed by `UrlBarCoordinator`).
2. During initialization, `LocationBarCoordinator` sets up a listener so that text changes in
   `UrlBarCoordinator` directly call `AutocompleteCoordinator.onTextChanged()`.
3. `AutocompleteCoordinator.onTextChanged()` forwards the call to
   `AutocompleteMediator.onTextChanged()`.
4. The `AutocompleteMediator` takes the user's input and initiates a request to the shared C++
   `AutocompleteController` through the JNI bridge.
5. When results are ready, the C++ `AutocompleteController` sends them back to the
   `AutocompleteMediator` via its `onSuggestionsReceived` JNI callback.
6. The `AutocompleteMediator` receives the `AutocompleteResult`, processes it, builds a list of view
   models, and updates the main `PropertyModel` for the suggestion list.
7. The `PropertyModel` change is observed by the
   [`OmniboxSuggestionsDropdownAdapter`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/suggestions/OmniboxSuggestionsDropdownAdapter.java),
   which then renders the final views in the suggestion dropdown list.
