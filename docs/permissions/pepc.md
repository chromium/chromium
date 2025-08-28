# Technical README: The `<permission>` Element (PEPC)

This document provides a detailed technical overview of the `<permission>` element feature, also known as PEPC (Permission Element Policy Control). It is intended for developers who want to understand the implementation of this feature in Chromium.

## 1. Overview

The `<permission>` element is an HTML element that allows websites to embed a permission request UI directly into their content. The goal is to make permission requests more contextual, user-initiated, and trustworthy, leading to a better user experience and higher permission grant rates.

The core principle of the feature is a two-step permission request process:
1.  The user clicks the `<permission>` element on the page.
2.  The browser displays a secondary, browser-controlled UI (a bubble prompt on desktop, a dialog on Android) to confirm the permission request.

This two-step process is crucial for security, as it prevents websites from tricking users into granting permissions with a single click.

## 2. Architectural Overview

The following diagram illustrates the high-level architecture of the `<permission>` element feature:

```
+-------------------------------------------------------------------------------------------------+
| Renderer Process (Blink)                                                                        |
|                                                                                                 |
|  +-------------------------+                                                                    |
|  | HTMLPermissionElement   |                                                                    |
|  +-------------------------+                                                                    |
|              |                                                                                  |
|              | 1. RegisterPageEmbeddedPermissionControl                                         |
|              | 3. RequestPageEmbeddedPermission                                                 |
|              v                                                                                  |
|  +-------------------------+                                                                    |
|  | PermissionService       |                                                                    |
|  +-------------------------+                                                                    |
|                                                                                                 |
+-------------------------------------------------------------------------------------------------+
               |                                                                                  |
               | Mojo IPC                                                                         |
               v                                                                                  |
+-------------------------------------------------------------------------------------------------+
| Browser Process                                                                                 |
|                                                                                                 |
|  +-----------------------+      +--------------------------+      +-------------------------+   |
|  | PermissionServiceImpl |----->| PermissionControllerImpl |----->| PermissionPromptFactory |   |
|  +-----------------------+      +--------------------------+      +-------------------------+   |
|              |                                                            |                     |
|              | 2. OnEmbeddedPermissionControlRegistered                   | 4. Create           |
|              v                                                            v                     |
|  +---------------------------------+                             +--------------------------+   |
|  | EmbeddedPermissionControlClient |                             | EmbeddedPermissionPrompt |   |
|  +---------------------------------+                             +--------------------------+   |
|                                                                                 |               |
|                                                                5. Create & Show |               |
|                                                                                 v               |
|                                                            +----------------------------------+ |
|                                                            | EmbeddedPermissionPromptBaseView | |
|                                                            +----------------------------------+ |
|                                                                                                 |
+-------------------------------------------------------------------------------------------------+
```

## 3. Component Deep Dive

The implementation of the `<permission>` element spans across the renderer (Blink), the browser process, and the UI.

### 3.1. `HTMLPermissionElement`: The Renderer Component

The renderer-side implementation is responsible for parsing the `<permission>` element, handling its attributes, and managing its state.

**[`third_party/blink/renderer/core/html/html_permission_element.h`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/html/html_permission_element.h) / `.cc`**

*   **[`HTMLPermissionElement`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/html/html_permission_element.h) class:** This is the C++ implementation of the `<permission>` element. It inherits from `HTMLElement` and handles all renderer-side logic for the element.
*   **Attributes:** It handles the `type` attribute, which specifies the permission to be requested (e.g., "camera", "geolocation").
*   **Communication:** It communicates with the browser process via the `PermissionService` Mojo interface.

The `HTMLPermissionElement` has several complex areas in its implementation, primarily due to the need for robust security and state management.

#### 3.1.1. Clickjacking Mitigation

The most complex part of the `HTMLPermissionElement` is its defense against clickjacking. This is achieved through a combination of an `IntersectionObserver` and geometry checks.

*   **`IntersectionObserver`:** The element uses an `IntersectionObserver` to monitor its visibility and occlusion, which is initialized in `HTMLPermissionElement::AttachLayoutTree`. The observer is configured to track whether the element is at least 90% visible and has been for at least 100ms. The callback for the observer is `HTMLPermissionElement::OnIntersectionChanged`.
*   **Geometry Checks:** The element also tracks its own geometry (size and position). The `HTMLPermissionElement::DidRecalcStyle` and `HTMLPermissionElement::DidFinishLifecycleUpdate` methods check if the element's intersection with the viewport has changed. If it has, a timer is started by calling `DisableClickingTemporarily`, and clicks are ignored until the timer has finished.
*   **Occlusion Detection:** The `IntersectionObserver` is configured with `expose_occluder_id = true` in its parameters. This allows the element to know which element is covering it via the `IntersectionObserverEntry` in the `OnIntersectionChanged` callback.

#### 3.1.2. State Management

The `HTMLPermissionElement` has a complex state machine that tracks the permission status, the element's validity, and the user's interaction with the element.

*   **Permission Status:** The element's appearance and behavior change based on the current permission status (granted, denied, or prompt). This status is cached in the renderer process (propagated from browser process early when the page completed navigation) and updated by the browser process via the `OnPermissionStatusChange` method, which is part of the `CachedPermissionStatus::Client` interface implemented by `HTMLPermissionElement`.
*   **Element Validity:** The element can be in an invalid state for several reasons. The `isValid()` method returns the element's validity, which is determined by the `GetClickingEnabledState()` method. This method checks for things like being in a secure context, having a valid `type` attribute, and having valid CSS.
*   **User Interaction:** The element tracks user interaction to prevent abuse. For example, in `HTMLPermissionElement::DefaultEventHandler`, it checks the `pending_request_created_` timestamp to see if a request is already pending.

#### 3.1.3. Shadow DOM and Internal Structure

The `<permission>` element is not a simple element. It's composed of a shadow DOM that contains an icon and a text span, which are created in the `HTMLPermissionElement::DidAddUserAgentShadowRoot` method.

*   **Shadow DOM:** The use of a shadow DOM encapsulates the element's internal structure and styling, preventing it from being manipulated by the website's CSS.
*   **Icon:** The icon is implemented as a separate custom element, [`HTMLPermissionIconElement`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/html/html_permission_icon_element.h), which is added to the shadow DOM. This allows for the icon to have its own dedicated logic and styling.
*   **Internal Styling:** The element has its own [internal stylesheet](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/html/resources/permission.css) that defines the appearance of the icon and text. This stylesheet is carefully constructed to be resistant to tampering.

#### 3.1.4. Mojo Communication and Lifecycle

The `HTMLPermissionElement` has a complex lifecycle that is tied to its communication with the browser process.

*   **Registration:** When the element is rendered on the page, `HTMLPermissionElement::InsertedInto` is called, which in turn calls `MaybeRegisterPageEmbeddedPermissionControl`. This method is responsible for making the `RegisterPageEmbeddedPermissionControl` Mojo call to the browser process.
*   **Connection Errors:** The `permission_service_` mojo remote has a disconnect handler set in `GetPermissionService()` to call `OnPermissionServiceConnectionFailed`, which resets the service connection.
*   **Garbage Collection:** The element's lifecycle is managed by the Blink garbage collector. The `HTMLPermissionElement::RemovedFrom` method handles cleanup, including unregistering the element from the browser process by calling `EnsureUnregisterPageEmbeddedPermissionControl`.

#### 3.1.5. CSS Validation

To prevent websites from styling the `<permission>` element in a deceptive way, the element validates its own CSS.

*   **Restricted Properties:** Only a limited set of CSS properties can be applied to the element. This is enforced by `HTMLPermissionElement::GetCascadeFilter()`, which filters out properties that are not marked as `kValidForPermissionElement`.
*   **Validation Logic:** The `HTMLPermissionElement::DidRecalcStyle` method calls `IsStyleValid()` to check the values of the allowed CSS properties. If an invalid value is detected, the element is disabled by calling `DisableClickingIndefinitely(DisableReason::kInvalidStyle)`.

#### 3.1.6. Fallback Mode

The `HTMLPermissionElement` includes a graceful degradation mechanism known as "fallback mode."

*   **Trigger:** This mode is activated if the element's `type` attribute is invalid or requests a permission that is not supported. This is checked in `HTMLPermissionElement::AttributeChanged`, and if the type is invalid, `EnableFallbackMode()` is called.
*   **Behavior:** When in fallback mode, the element essentially reverts to behaving like a generic `HTMLUnknownElement`. The `EnableFallbackMode()` method achieves this by adding an `<slot>` element to the user agent shadow root, which allows the element's children to render, and removing the internal permission container.

### 3.2. Mojo (Communication Layer)

The Mojo interface defines the contract for communication between the renderer and the browser process.

**[`third_party/blink/public/mojom/permissions/permission.mojom`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/permissions/permission.mojom)**

*   **[`PermissionService`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/permissions/permission.mojom) interface:** Implemented in the browser process by `PermissionServiceImpl`, this interface provides methods for the renderer to interact with the permission system. The key methods for the `<permission>` element are:
    *   `RegisterPageEmbeddedPermissionControl`: The renderer calls this to inform the browser about a new `<permission>` element.
    *   `RequestPageEmbeddedPermission`: The renderer calls this when the user clicks the `<permission>` element.
*   **[`EmbeddedPermissionControlClient`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/permissions/permission.mojom) interface:** Implemented in the renderer process by `HTMLPermissionElement`, this interface allows the browser to send messages back to the permission element. The key method is:
    *   `OnEmbeddedPermissionControlRegistered`: Called by the browser to inform the renderer of the result of the registration process.

### 3.3. Browser Process

The browser-side implementation is responsible for handling the permission logic, displaying the UI, and managing the user's decisions.

#### 3.3.1. Instance-per-Page Logic

A key security and anti-abuse feature of PEPC is that it limits the number of `<permission>` elements that can be active on a single page at any given time. This logic is managed by the `EmbeddedPermissionControlChecker` class.

**[`content/browser/permissions/embedded_permission_control_checker.h`](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/permissions/embedded_permission_control_checker.h) / `.cc`**

*   **`EmbeddedPermissionControlChecker` class:** This class is attached to a `Page` object (making it "per page"). It's responsible for tracking and enforcing the instance limit for `<permission>` elements.
*   **`client_map_` Data Structure:** The core of the checker is the `client_map_`, a `std::map` that associates a set of permission types with a queue of `Client` objects. Each `Client` object represents a single `<permission>` element on the page.
*   **`kMaxPEPCPerPage` Limit:** The checker enforces a hard limit of 3 active `<permission>` elements per permission type on a page. This is defined by the `kMaxPEPCPerPage` constant.
*   **Queuing Behavior:** When a new `<permission>` element is registered, `CheckPageEmbeddedPermission` is called. If the number of active elements for that permission type is less than the limit, the new element is approved. Otherwise, it's added to the queue but not approved. When an active element is removed (e.g., the user navigates away or the element is removed from the DOM), its corresponding `Client` is removed from the queue, and the next element in the queue is approved. This creates a queuing behavior that ensures that only a limited number of elements are active at any given time.

**[`content/browser/permissions/permission_service_impl.h`](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/permissions/permission_service_impl.h) / `.cc`**

*   **[`PermissionServiceImpl`](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/permissions/permission_service_impl.h) class:** This class is the browser-side implementation of the `PermissionService` Mojo interface.
*   **Request Dispatching:** It acts as a dispatcher, receiving `RegisterPageEmbeddedPermissionControl` and `RequestPageEmbeddedPermission` calls from the renderer and forwarding them to the `PermissionControllerImpl`.
*   **Validation:** In the `RegisterPageEmbeddedPermissionControl` method, it uses `EmbeddedPermissionControlChecker` to perform security checks on the element.
*   **Context Management:** It holds a `PermissionServiceContext` member, which provides information about the renderer frame and the browser context.

**[`content/browser/permissions/permission_controller_impl.h`](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/permissions/permission_controller_impl.h) / `.cc`**

*   **[`PermissionControllerImpl`](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/permissions/permission_controller_impl.h) class:** This is the central class for permission management in the browser process. It handles permission requests by creating `PermissionRequest` objects, queries permission statuses, and stores the user's decisions.

### 3.4. UI Components

The UI components are responsible for displaying the secondary permission prompt to the user. The logic for determining which UI screen to show is shared between platforms, while the view implementation is platform-specific.

#### 3.4.1. `EmbeddedPermissionPromptFlowModel`: Shared UI Logic

The core logic for the embedded permission prompt's multi-screen flow is managed by the `EmbeddedPermissionPromptFlowModel`. This class is not a UI view itself, but a shared model used by both desktop and Android to decide which screen (or "variant") of the prompt to display.

**[`components/permissions/embedded_permission_prompt_flow_model.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/embedded_permission_prompt_flow_model.h) / `.cc`**

*   **State Management:** The model inspects the current state of the permission (e.g., `ContentSetting`, OS-level status, enterprise policies) to determine the appropriate UI.
*   **Screen Variants:** It defines an enum, `Variant`, which represents all possible screens the prompt can show. This includes states like `kAsk` (the main prompt), `kPreviouslyDenied` (a prompt variant for previously denied permissions), `kOsPrompt` (informing the user that an OS prompt is active), and `kAdministratorDenied` (informing the user that the permission is blocked by policy).
*   **Platform Agnostic:** Both the desktop and Android UI controllers create an instance of this model and use its `DeterminePromptVariant` method to decide which view to present to the user. This keeps the complex flow logic consistent across platforms.

#### 3.4.2. Desktop (Views)

**[`chrome/browser/ui/views/permissions/permission_prompt_factory.cc`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/permissions/permission_prompt_factory.cc)**

*   **`CreatePermissionPrompt` function:** This factory function is responsible for creating the correct type of permission prompt.
*   **Prompt Selection:** It checks if the permission request was initiated by a `<permission>` element by calling `permissions::PermissionUtil::ShouldCurrentRequestUsePermissionElementSecondaryUI`. If it was, it creates an `EmbeddedPermissionPrompt`.

**[`chrome/browser/ui/views/permissions/embedded_permission_prompt.h`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/permissions/embedded_permission_prompt.h) / `.cc`**

*   **[`EmbeddedPermissionPrompt`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/permissions/embedded_permission_prompt.h) class:** This is the high-level controller for the embedded permission prompt. It's not a view itself, but it manages the lifecycle of the prompt view.
*   **Flow Management:** The [`EmbeddedPermissionPromptFlowModel`](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/embedded_permission_prompt_flow_model.h) is used to manage the different views that can be shown in the prompt. This includes the main prompt view, a view that informs the user that the permission is blocked, and a view that informs the user that the permission has been granted. This state management adds a layer of complexity to the prompt's implementation.
*   **Delegate:** It acts as a delegate for the prompt view, handling user actions like "Allow" and "Dismiss", which in turn call `Accept()` and `Dismiss()` on the `PermissionRequest` delegate.

**[`chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h) / `.cc`**

*   **[`EmbeddedPermissionPromptBaseView`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h) class:** This is the abstract base class for the actual UI of the embedded permission prompt. It's a `views::BubbleDialogDelegateView` that provides the common structure for the prompt.
*   **Dynamic UI:** The [`EmbeddedPermissionPromptBaseView`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h) is a highly dynamic view. The text and buttons that are displayed can change based on the state of the permission request. This is handled by the `GetRequestLinesConfiguration()` and `GetButtonsConfiguration()` methods, which are implemented by subclasses.

#### 3.4.2. Android

On Android, the permission prompt is implemented as a modal dialog. The differentiation between a regular prompt and a PEPC prompt is handled by a boolean flag that is passed from the C++ side to the Java side.

**[`components/permissions/android/permission_prompt/permission_prompt_android.cc`](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/android/permission_prompt/permission_prompt_android.cc)**

*   **[`PermissionPromptAndroid`](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/android/permission_prompt/permission_prompt_android.h) class:** This is the C++ part of the Android permission prompt. It acts as a bridge to the Java UI.
*   **PEPC Detection:** In its constructor, it calls `permissions::PermissionUtil::ShouldCurrentRequestUsePermissionElementSecondaryUI()` to determine if the current permission request is for a PEPC prompt.
*   **JNI Bridge:** It passes the result of the PEPC check as a boolean (`useRequestElement`) to the Java `PermissionDialogController` when it's created via a JNI call.

**[`components/permissions/android/java/src/org/chromium/components/permissions/PermissionDialogController.java`](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/android/java/src/org/chromium/components/permissions/PermissionDialogController.java)**

*   **[`PermissionDialogController`](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/android/java/src/org/chromium/components/permissions/PermissionDialogController.java) class:** This is the main controller for the permission prompt on Android.
*   **PEPC Flag:** It receives the `useRequestElement` boolean in its constructor. This flag is then used to customize the dialog for a PEPC prompt (e.g., by changing the text or the layout).
*   **Lifecycle Management:** The [`PermissionDialogController`](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/android/java/src/org/chromium/components/permissions/PermissionDialogController.java) has to carefully manage the lifecycle of the `PermissionDialog`. It needs to handle cases where the dialog is dismissed by the user, the system, or by a screen rotation.
*   **JNI Bridge:** The controller communicates with the C++ backend through a JNI bridge (`mNativePermissionDialogController`). This involves passing data between the Java and C++ worlds, which can be complex and error-prone.

**[`components/permissions/android/java/src/org/chromium/components/permissions/PermissionDialogDelegate.java`](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/android/java/src/org/chromium/components/permissions/PermissionDialogDelegate.java)**

*   **[`PermissionDialogDelegate`](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/android/java/src/org/chromium/components/permissions/PermissionDialogDelegate.java) class:** This class acts as a bridge between the `PermissionRequest` (from the C++ side) and the Java UI. It provides the `PermissionDialogController` with the necessary data to display the prompt (e.g., `getMessageText()`, `getPrimaryButtonText()`) and forwards user decisions back to the `PermissionRequest` (e.g., `onAccept()`, `onDismiss()`).

The view for the permission prompt is defined in the layout files `permission_dialog_vertical_buttons_permission.xml` and `permission_dialog.xml`, and is inflated by the `PermissionDialogCoordinator`.

## 4. Code-Level Flow

This section describes the step-by-step code flow for a typical permission request initiated by a `<permission>` element.

1.  **Element Creation (Renderer):**
    *   When the HTML parser encounters a `<permission>` tag, it creates an `HTMLPermissionElement` instance.
    *   When the element is rendered on the page, its `MaybeRegisterPageEmbeddedPermissionControl` method is called.
    *   This method gets the `PermissionService` Mojo remote and calls `RegisterPageEmbeddedPermissionControl`, passing the requested permissions and a pending remote for its `EmbeddedPermissionControlClient` implementation.

2.  **Registration (Browser):**
    *   The `PermissionServiceImpl` receives the `RegisterPageEmbeddedPermissionControl` call.
    *   It uses `EmbeddedPermissionControlChecker` to verify that the element is allowed in the current context.
    *   It then calls `OnEmbeddedPermissionControlRegistered` on the `EmbeddedPermissionControlClient` remote to send the result back to the renderer.

3.  **User Interaction (Renderer):**
    *   The user clicks the `<permission>` element.
    *   The `HTMLPermissionElement`'s `DefaultEventHandler` calls `RequestPageEmbeddedPermissions`.
    *   This method calls `RequestPageEmbeddedPermission` on the `PermissionService` Mojo remote, passing the permission descriptors and the element's position.

4.  **Prompt Creation (Browser):**
    *   The `PermissionServiceImpl` receives the `RequestPageEmbeddedPermission` call.
    *   It forwards the request to the `PermissionControllerImpl`.
    *   The `PermissionControllerImpl` creates a `PermissionRequest` and shows it via the `PermissionRequestManager`.
    *   The `PermissionRequestManager` creates a `PermissionPromptAndroid` instance.
    *   Inside the `PermissionPromptAndroid` constructor, `permissions::PermissionUtil::ShouldCurrentRequestUsePermissionElementSecondaryUI()` is called.
    *   The `PermissionPromptAndroid` then creates a `PermissionDialogController` on the Java side, passing the boolean result of the check.

5.  **Prompt Display (Browser):**
    *   **Desktop:** The `EmbeddedPermissionPrompt` (the controller) creates a concrete subclass of `EmbeddedPermissionPromptBaseView` (the view). The view is shown as a bubble anchored to the position of the `<permission>` element.
    *   **Android:** The `PermissionDialogDelegate` holds the information about whether this is a PEPC prompt. The `PermissionDialogController` passes this delegate to the `PermissionDialogCoordinator`, which then inflates and customizes the appropriate view, for example by using a different layout.

6.  **User Decision (Browser):**
    *   The user clicks a button on the prompt (e.g., "Allow").
    *   **Desktop:** The `EmbeddedPermissionPromptBaseView` calls a method on its delegate, the `EmbeddedPermissionPrompt`. The `EmbeddedPermissionPrompt` calls the appropriate method on its `delegate_` (the `PermissionRequest`), such as `Accept()` or `Deny()`.
    *   **Android:** User actions on the dialog view are handled by the `PermissionDialogMediator`, which notifies the `PermissionDialogController`. The controller then calls the appropriate method on the `PermissionDialogDelegate`.

7.  **Decision Handling (Browser and Renderer):**
    *   The `PermissionRequest` notifies the `PermissionControllerImpl` of the user's decision.
    *   The `PermissionControllerImpl` stores the decision.
    *   The result of the `RequestPageEmbeddedPermission` Mojo call is sent back to the renderer, and the `HTMLPermissionElement`'s `OnEmbeddedPermissionsDecided` method is called with the result.



### 5.2. Bypassing the Quiet UI

The most significant difference is that PEPC requests bypass the "quiet" permission UI. The quiet UI is a less intrusive prompt that is shown when the browser determines that a regular permission prompt would be too disruptive.

## 5. Behavioral Differences: PEPC vs. Regular Requests

PEPC permission requests are treated differently from regular permission requests in a few key ways, primarily because they are considered a stronger signal of user intent.

### 5.1. Bypassing Permission Status Check

A key difference is that PEPC prompts can be shown even if the permission has been previously denied by the user. Regular permission prompts are only shown if the permission status is "ask".

*   **Logic:** Both regular and PEPC permission requests flow through the same initial path, eventually reaching `PermissionContextBase::RequestPermission` for the specific permission type. Inside this method, the following sequence occurs:
    1.  `GetPermissionStatus` is called to retrieve the current status of the permission (e.g., GRANTED, DENIED, or ASK). This function also returns the `source` of the status (e.g., user decision, embargo, kill switch).
    2.  The result is passed to `PermissionUtil::CanPermissionRequestIgnoreStatus`. This is the critical function for the bypass behavior.
    3.  `CanPermissionRequestIgnoreStatus` checks if the request was initiated by a permission element (via the `embedded_permission_element_initiated` flag in `PermissionRequestData`).
    4.  If it is a PEPC request, the function checks the `source` of the permission status. It returns `true` if the denial is from a "soft" source that a direct user gesture should be able to override (like a previous user dismissal or ignore). It returns `false` for "hard" denials (like a kill switch or enterprise policy).
    5.  Back in `PermissionContextBase::RequestPermission`, there is a check: `if (!status_ignorable && ...)`
        *   For a **regular request**, `CanPermissionRequestIgnoreStatus` returns `false`. If the permission is already denied, the condition is met, and the request is immediately rejected without showing a prompt.
        *   For a **PEPC request** that was previously denied by the user, `CanPermissionRequestIgnoreStatus` returns `true`. This makes the `!status_ignorable` part of the condition `false`, so the entire block is skipped.
    6.  Because the rejection block is skipped, the code proceeds to `DecidePermission`, which continues the process of showing a prompt to the user.

*   **Rationale:** This allows users to easily change their minds about a permission that they have previously denied, without having to go into site settings. The explicit user gesture on the `<permission>` element is considered a strong enough signal to warrant re-prompting, unless the permission is blocked by a system-level or administrative policy.

### 5.2. Bypassing the Quiet UI

The most significant difference is that PEPC requests bypass the "quiet" permission UI. The quiet UI is a less intrusive prompt that is shown when the browser determines that a regular permission prompt would be too disruptive.

*   **Logic:** The decision to bypass the quiet UI is made in [`PermissionManager::RequestPermissions`](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_manager.cc). This method checks if the request is a PEPC request by calling `permissions::PermissionUtil::ShouldCurrentRequestUsePermissionElementSecondaryUI`. If it is, the quiet UI is skipped and the full permission prompt is shown.
*   **Rationale:** Because a PEPC request is initiated by a direct user interaction with the `<permission>` element, it's assumed that the user is expecting a prompt and is ready to make a decision. Therefore, the less intrusive quiet UI is not necessary.

### 5.3. UI Customization

As mentioned in the previous sections, the UI for a PEPC prompt is different from a regular permission prompt.

*   **Desktop:** On desktop, a PEPC prompt is an `EmbeddedPermissionPrompt`, which is a bubble that is anchored to the `<permission>` element. A regular permission prompt is a bubble that is anchored to the omnibox.
*   **Android:** On Android, the `PermissionDialogController` receives a `useRequestElement` boolean that tells it to customize the dialog for a PEPC request. This can include changing the text to be more contextual to the user's action, or using a different layout.

### 5.4. Request Preemption

PEPC requests have a higher priority than regular permission requests. If a regular permission prompt is already showing and a PEPC request is triggered, the regular prompt will be cancelled and the PEPC prompt will be shown immediately.

*   **Logic:** This logic is implemented in the [`PermissionRequestQueue::AddRequest`](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_request_queue.cc). When a new request is added to the queue, it checks if the new request is a PEPC request and if the currently showing request is *not* a PEPC request. If both are true, the currently showing request is cancelled.
*   **Rationale:** This ensures that PEPC requests, which are the result of a direct and explicit user action, are not blocked by less urgent, programmatically triggered permission requests.

## 6. Metrics

The PEPC feature is instrumented with a number of metrics to monitor its usage and performance. These metrics are recorded in UMA and UKM.

### 6.1. Renderer Metrics

These metrics are recorded in the renderer process and are related to the `<permission>` element itself.

*   **`Blink.PermissionElement.UserInteractionAccepted`**: A boolean histogram that records whether a user interaction (click) on the `<permission>` element was accepted or not. A click might be rejected for security reasons, for example if the element is obscured or has a deceptive style.

*   **`Blink.PermissionElement.UserInteractionDeniedReason`**: An enumeration histogram that records the reason why a user interaction was denied. The possible values are:
    *   `kInvalidType`: The `type` attribute of the element is invalid.
    *   `kFailedOrHasNotBeenRegistered`: The element has not been successfully registered with the browser process.
    *   `kRecentlyAttachedToLayoutTree`: The element was recently attached to the layout tree.
    *   `kIntersectionWithViewportChanged`: The element's intersection with the viewport has recently changed.
    *   `kIntersectionVisibilityOutOfViewPortOrClipped`: The element is outside the viewport or clipped.
    *   `kIntersectionVisibilityOccludedOrDistorted`: The element is occluded by another element or has a distorted visual effect.
    *   `kInvalidStyle`: The element has a deceptive style.
    *   `kUntrustedEvent`: The click event was not triggered by a real user.

*   **`Blink.PermissionElement.InvalidStyleReason`**: An enumeration histogram that records the reason why the element's style was considered invalid.

### 6.2. Browser Metrics

These metrics are recorded in the browser process and are related to the PEPC prompt.

*   **`Permissions.EmbeddedPermissionPrompt.Flow.Variant`**: An enumeration histogram that records which variant of the PEPC prompt was shown to the user. The possible values are:
    *   `kUninitialized`
    *   `kAdministratorGranted`
    *   `kPreviouslyGranted`
    *   `kOsSystemSettings`
    *   `kOsPrompt`
    *   `kAsk`
    *   `kPreviouslyDenied`
    *   `kAdministratorDenied`

*   **`Permissions.Prompt.{PermissionType}.ElementAnchoredBubble.{OsScreen}.OsScreenAction`**: An enumeration histogram that records the user's action on the OS-related screens of the PEPC prompt.
    *   `{PermissionType}` can be `Camera`, `Microphone`, or `Geolocation`.
    *   `{OsScreen}` can be `OsPrompt` or `OsSystemSettings`.
    *   `OsScreenAction` can be:
        *   `kSystemSettings`
        *   `kDismissedXButton`
        *   `kDismissedScrim`
        *   `kOsPromptDenied`
        *   `kOsPromptAllowed`

*   **`Permissions.Prompt.{PermissionType}.ElementAnchoredBubble.{OsScreen}.{OsScreenAction}.TimeToAction`**: A time histogram that records the time between showing an OS-related screen and the user taking an action on it.

### 6.3. UKM

UKM data is recorded for the PEPC feature to allow for more detailed analysis of user behavior. The `ElementAnchoredPermissionPrompt` event is recorded with the following metrics:

*   **`Action`**: The user's action on the prompt. The possible values are defined in the `ElementAnchoredBubbleAction` enum.
*   **`Variant`**: The variant of the prompt that was shown. The possible values are defined in the `ElementAnchoredBubbleVariant` enum.
*   **`ScreenCounter`**: The number of screens that were shown to the user during the permission request flow.
