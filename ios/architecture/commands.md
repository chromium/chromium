# UI Commands and Disintermediated Dispatch

> **Core Principle:** In order for encapsulated coordinators to trigger UI
> changes outside of their own scope without coupling components together, iOS
> Chrome sends **UI Commands** via **Disintermediated Dispatch**.

## Architectural Motivation

In iOS Chrome, **coordinators** encapsulate self-contained UI features (often
managing a **mediator** for model interaction and a **view controller** for view
management). Frequently, a feature or model-layer component needs to request a
UI change outside of its own scope—for example, presenting the Settings UI or
opening the Reading List.

Because of encapsulation and decoupling:

*   The requesting component must not be responsible for presenting or managing
    another feature's UI.
*   The requesting component must not know *which* coordinator satisfies the
    request, nor the presentation details of *how* that UI is displayed.
*   The satisfying coordinator must not be coupled to the various components
    that can request its UI.
*   Requests should not be manually relayed up or down an object hierarchy
    through intermediate delegate protocols. Any object that needs to request a
    UI change should hold an explicit, narrow dependency on an interface that
    can satisfy it.

While UIKit's responder chain (an implementation of the *Chain of
Responsibility* pattern) routes events without coupling senders to receivers, it
cannot be reconfigured outside of the `UIView` and `UIViewController` hierarchy.
Using the responder chain would force `UIViewController` instances to handle
cross-feature routing, violating iOS Chrome's separation between view
controllers (which strictly manage views) and coordinators (which manage feature
flow and navigation).

More generally, *Chain of Responsibility* and *Tree of Responsibility* patterns
rely on intermediary objects relaying messages and allow any participant along
the path to intercept or override a message. By contrast, iOS Chrome uses
**Disintermediated Dispatch**: a message is routed directly to a single
runtime-configured target regardless of where in the application the message was
sent from. This guarantees that invoking a command has the same effect
everywhere within its scope, making application behavior predictable and easier
to reason about.

---

## Overview

**UI Commands** form the public API of the UI layer. To trigger a UI change
outside the current component's scope:

1.  Define a **UI command**—an Objective-C method returning `void`—inside a
    functional **command protocol**.
2.  Configure the `Browser`-scoped **command dispatcher** (`CommandDispatcher`)
    in the owning coordinator to forward selectors from that protocol to the
    target object that implements them.
3.  Create a type-safe, runtime-checked **command handler** using
    `HandlerForProtocol()` and inject that handler (`id<SomeCommands>`) as an
    opaque dependency into any object that needs to invoke those commands.
4.  When the handling coordinator stops, unregister it from the dispatcher.

---

## Commands and Command Protocols

A **UI command** is an Objective-C method that requests a high-level UI change.
Related commands are grouped into an Objective-C **command protocol** named for
their shared functional domain (`<Feature>Commands`).

```objectivec
// Commands related to the Page Info UI.
@protocol PageInfoCommands

// Shows the page security info for the current page.
- (void)showPageInfo;

// Hides the page security info.
- (void)hidePageInfo;

// Shows the security help page.
- (void)showSecurityHelpPage;

@end
```

### Characteristics of a UI Command

*   **UI-only:** Commands are exclusively for inducing UI changes (typically
    presenting or dismissing a UI feature). Operations that mutate model
    state—including core web navigation actions such as loading a URL,
    navigating back/forward, or mutating a `WebStateList`—must not use UI
    commands. Similarly, UI updates driven by model changes belong in the
    **mutation-observation cycle** (via model observers, mediators, and consumer
    protocols), not commands.
*   **Context provided by the receiver:** A command is a high-level instruction
    for which the recipient, not the sender, supplies most of the execution
    context. For example, `-printTab` does not specify which tab to print or
    which view controller should present the print UI; the receiving coordinator
    already has access to the active tab and presentation context.
*   **Minimal, model-level parameters:** Commands should take as few parameters
    as possible, and often none. When parameters are necessary, they should be
    model-layer objects or simple value types that provide context for the UI
    feature being displayed. Commands must **not** pass UI-layer objects
    (`UIView*`, `UIViewController*`) or screen geometry (`CGPoint`, `CGRect`)
    that dictate *how* or *where* the feature is presented relative to the rest
    of the UI.
*   **Command objects for complex parameters:** When a command requires more
    than three parameters, or has multiple variants taking different combinations
    of model-layer parameters, bundle those parameters into a **command object**
    (an Objective-C class named `<Verb><Feature>Command` that encapsulates the
    payload, following the classic *Command* pattern) and pass that object as
    the sole parameter to a single command method. This allows the parameter set
    to evolve without updating every call site. Command object headers and
    implementations live in the same directory as the command protocols that use
    them:
    ```objectivec
    // generate_qr_code_command.h
    @interface GenerateQRCodeCommand : NSObject

    - (instancetype)initWithURL:(const GURL&)URL
                          title:(NSString*)title NS_DESIGNATED_INITIALIZER;
    - (instancetype)init NS_UNAVAILABLE;

    @property(nonatomic, readonly) const GURL& URL;
    @property(nonatomic, copy, readonly) NSString* title;

    @end

    // qr_generation_commands.h
    @protocol QRGenerationCommands <NSObject>

    // Generates a QR code based on the `command` properties and displays it.
    - (void)showQRCode:(GenerateQRCodeCommand*)command;

    // Dismisses the view showing a QR code, if present.
    - (void)hideQRCode;

    @end
    ```
*   **No return values:** Command methods must always return `void`. Commands
    are one-way requests, not queries. Completion or callback blocks as
    command arguments are discouraged because they act as asynchronous return
    values and couple the sender to the receiving coordinator's presentation
    lifecycle; they should only be used as a last resort when synchronizing
    flows cannot be achieved otherwise.
*   **Functional grouping:** Group commands into protocols according to what
    feature or function they control (e.g., `SettingsCommands`,
    `PageInfoCommands`), never according to which class happens to implement
    them.
*   **No optional methods:** All methods in a command protocol must be
    `@required` (the default). Command protocols must never declare `@optional`
    methods.
*   **No protocol inheritance:** Command protocols must be flat (`@protocol
    FooCommands` or `@protocol FooCommands <NSObject>`); a command protocol must
    never inherit from another command protocol.

### Shared vs. Local Command Protocols

*   **Shared command protocols:** Any command protocol that can be invoked across
    features must be defined in its own header in
    `//ios/chrome/browser/shared/public/commands/` (along with any command
    object headers and `.mm` implementation files it uses). Every protocol in
    this directory must be dispatched through `CommandDispatcher`, and *only*
    protocols in this directory may be dispatched through `CommandDispatcher`.
*   **Local command protocols:** Within a single feature, it is often useful to
    define a command-style protocol to trigger purely internal UI changes (such
    as showing or hiding sub-parts of that feature's UI). If no other feature
    needs to call these methods, define a **local command protocol** inside the
    feature's own directory rather than `shared/public/commands/`. Local
    command protocols are not registered with `CommandDispatcher`; instead, the
    implementing object is passed directly as the `id<LocalFeatureCommands>`
    handler without `HandlerForProtocol()`. Local command protocols otherwise
    follow all command and handler naming conventions.

---

## The Command Dispatcher

In iOS Chrome, disintermediated dispatch is provided by the `CommandDispatcher`
class. A `CommandDispatcher` is a **Browser-scoped** object owned by each
`Browser` instance and accessed via `browser->GetCommandDispatcher()`.

### Configuring and Stopping Dispatch

`CommandDispatcher` provides methods to start and stop routing commands to a
target object:

```objectivec
- (void)startDispatchingToTarget:(id)target forProtocol:(Protocol*)protocol;
- (void)stopDispatchingForProtocol:(Protocol*)protocol;

- (void)stopDispatchingToTarget:(id)target;
```

Dispatching to a `target` for a `protocol` configures the dispatcher to route
all required methods defined in that protocol to `target`.

`CommandDispatcher` enforces the following runtime contracts:

*   **Weak target references:** `CommandDispatcher` holds weak references to
    registered target objects.
*   **No duplicate registration:** Attempting to register a protocol (or any
    method within it) that is already being dispatched is a programming error
    and triggers an assertion failure (`CHECK`).
*   **Unregistered commands:** Sending a message to the dispatcher for a command
    that is not currently registered (or after dispatch for it has been stopped)
    behaves like messaging an unknown selector on any other Objective-C object,
    raising an `NSInvalidArgumentException`.

Coordinators are responsible for configuring the dispatcher and managing the
registration lifecycle:

*   **Choosing the target:** Coordinators are often the targets for commands
    themselves—for example, a parent coordinator might handle `-showReadingList`
    by creating and starting a `ReadingListCoordinator`. However, the target
    passed to `-startDispatchingToTarget:...` does not have to be `self`. If a
    mediator or helper object owned by the coordinator is the right place to
    handle a command, the coordinator configures the dispatcher to forward
    commands to that object instead.
*   **Self-registration by coordinators:** A coordinator must never set up
    dispatch targeting *another* coordinator. Instead, the child coordinator
    accesses the dispatcher through its own `Browser` reference and configures
    dispatch for itself. Because external callers never invoke commands on a
    coordinator directly, coordinators must never declare conformance to a
    command protocol in their public `.h` header; conformance belongs in the
    class extension inside the `.mm` implementation file.
*   **Deterministic registration and teardown:** A coordinator typically starts
    dispatching in `-start`. Because Objective-C object deallocation timing is
    not deterministic, any coordinator that registers command targets **must**
    explicitly unregister them in `-stop` (using `-stopDispatchingToTarget:` or
    `-stopDispatchingForProtocol:`) rather than relying on `-dealloc`.

A typical coordinator implementation configures and tears down dispatch as
follows:

```objectivec
// PageInfoCoordinator.mm
@interface PageInfoCoordinator () <PageInfoCommands>
@end

@implementation PageInfoCoordinator

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(PageInfoCommands)];
  // ... create view controller / mediator and present UI ...
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  // ... clean up view controller / mediator ...
}

#pragma mark - PageInfoCommands

- (void)showPageInfo {
  // ...
}

- (void)hidePageInfo {
  // ...
}

- (void)showSecurityHelpPage {
  // ...
}

@end
```

---

## Type Safety and Command Handlers

Although `CommandDispatcher` forwards registered command selectors at runtime,
it does not statically conform to command protocols at compile time. Calling a
command method directly on a `CommandDispatcher*` will fail to compile.

Instead, callers invoke commands through a **command handler**—an opaque
reference of type `id<SomeCommands>`. For shared command protocols, coordinators
obtain a handler from the dispatcher using the `HandlerForProtocol()` macro
(`HandlerForProtocol` is defined as part of the `CommandDispatcher`
[header file](/ios/chrome/browser/shared/public/commands/command_dispatcher.h)):

```objectivec
id<PageInfoCommands> pageInfoHandler =
    HandlerForProtocol(self.browser->GetCommandDispatcher(), PageInfoCommands);
[pageInfoHandler showSecurityHelpPage];
```

`HandlerForProtocol(dispatcher, ProtocolName)` verifies at runtime (`CHECK`)
that `dispatcher` is currently dispatching for `ProtocolName` and returns
`dispatcher` cast to `id<ProtocolName>`.

### Passing Dispatchers vs. Passing Handlers

`Browser` and `CommandDispatcher` provide broad access to the entire command
routing system, whereas `id<SomeCommands>` handlers expose only a single
command protocol. To keep dependencies narrow, explicit, and easy to mock in
tests, `Browser` and `CommandDispatcher` are restricted to the coordinator
layer:

*   **Inside the coordinator layer:** Coordinators are initialized with a
    `Browser*` and obtain the `CommandDispatcher*` via
    `self.browser->GetCommandDispatcher()`. Never pass command handlers into an
    object that is already initialized with a `Browser*`, as doing so is
    redundant—it can generate the handlers it needs directly from the
    `Browser`'s dispatcher.
*   **Outside the coordinator layer:** Never pass `Browser*` or
    `CommandDispatcher*` into mediators, view controllers, views, or model
    objects, as that would allow them to configure dispatch or invoke arbitrary
    commands without declaring those dependencies. Instead, coordinators extract
    the specific command handlers those objects require using
    `HandlerForProtocol()` and inject them as explicit `id<SomeCommands>`
    dependencies.
*   **Never bypass the dispatcher for shared command protocols:** Do not pass an
    object that happens to conform to a shared command protocol (such as `self`)
    directly as a handler. Always pass the `CommandDispatcher` coerced via
    `HandlerForProtocol()` so that calls go through the dispatcher's runtime
    checks and registration lifecycle.

### Invoking Commands Across Layers

Because UI commands are the public API of the UI layer, any object that needs to
trigger a UI change—including view controllers, views, mediators, tab helpers,
and agents—may hold weak references to injected command handlers and call
commands on them.

```objectivec
// PageInfoViewController.h
@interface PageInfoViewController : UIViewController

@property(nonatomic, weak) id<PageInfoCommands> pageInfoHandler;

@end

// PageInfoViewController.mm
@implementation PageInfoViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  __weak __typeof(self) weakSelf = self;
  UIAction* helpAction = [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf.pageInfoHandler showSecurityHelpPage];
  }];
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  UIButton* helpButton = [UIButton buttonWithConfiguration:buttonConfiguration
                                             primaryAction:helpAction];
  // ...
}

@end
```

Because `PageInfoViewController` depends only on `id<PageInfoCommands>`, it is
completely decoupled from `CommandDispatcher` and from the coordinator that
handles the command. In unit tests, a mock or fake conforming to
`PageInfoCommands` can be injected directly.

---

## Naming, Protocol Composition, and Delegation Guidelines

### Naming Command Handlers

*   **Property and ivar names:** Name handler properties and instance variables
    `<name>Handler`, where `<name>` is the command protocol name *without* the
    trailing `Commands` suffix:
    ```objectivec
    // Correct:
    @property(nonatomic, weak) id<PageInfoCommands> pageInfoHandler;
    @property(nonatomic, weak) id<BrowserCommands> browserHandler;

    // Incorrect:
    @property(nonatomic, weak) id<PageInfoCommands> pageInfoCommandsHandler;
    @property(nonatomic, weak) id<PageInfoCommands> handler;
    @property(nonatomic, weak) id<PageInfoCommands> dispatcher;
    ```
*   **Local variables and parameters:** Within the narrow scope of a single
    method parameter or local variable, the shorter name `handler` is acceptable
    when unambiguous.
*   **Never name a handler `dispatcher`:** Only a `CommandDispatcher*` should be
    named `dispatcher`.
*   **Weak references:** Handler properties and ivars must always be `weak` to
    avoid retain cycles (especially when the handler is a local command
    protocol implementation or a test double). Note that for handlers obtained
    via `HandlerForProtocol()`, the underlying object is the `Browser`'s
    `CommandDispatcher`; therefore, the `weak` handler reference remains
    non-`nil` for the lifetime of the `Browser`, even after a target
    coordinator stops dispatching.

### Do Not Inherit or Compose Command Protocols

Command protocols must not inherit from other command protocols, and callers
must not combine multiple command protocols into a single handler property or
create synthetic composed protocols for handler dependencies. Declare a separate
handler for each command protocol an object requires:

```objectivec
// Correct:
@property(nonatomic, weak) id<PageInfoCommands> pageInfoHandler;
@property(nonatomic, weak) id<BrowserCommands> browserHandler;

// Incorrect (combining protocols in a single handler):
@property(nonatomic, weak) id<PageInfoCommands, BrowserCommands> handler;

// Incorrect (inheriting from another command protocol):
@protocol PageInfoCommands <BrowserCommands>
@end
```

### Commands vs. Other Protocol Patterns

*   **Child-to-parent coordinator communication:** Because coordinators
    encapsulate independent features, delegating upward from a child coordinator
    to its parent coordinator via a custom delegate protocol is discouraged. Use
    command dispatch instead.
*   **Intra-feature communication:** Within a single feature, use **mutator**
    and **consumer** protocols for communication between a view controller and
    its mediator, or a **local command protocol** for purely internal UI
    actions.
*   **Avoid "Middle Man" forwarding:** Do not create a delegate protocol whose
    only purpose is to forward a call to a command handler in the same layer.
    For example, if a custom subview has a button that triggers `-hideFoo` on
    `id<FooCommands>`, pass `fooHandler` directly to the subview rather than
    defining a `FooSubviewDelegate` on the view controller just to relay the
    action to `fooHandler`.

---

## Glossary

*   **Command Handler:** An opaque object reference conforming to a single
    command protocol (`id<SomeCommands>`), passed into objects that need to
    invoke those commands. In production code, handlers for command protocols
    are created by typecasting a `CommandDispatcher` via
    `HandlerForProtocol()`; in tests, they are typically test doubles.
*   **Command Object:** An object that encapsulates multiple model-layer
    parameters passed as the single argument to a UI command (the *Command*
    pattern from *Design Patterns*).
*   **Command Protocol:** An Objective-C protocol grouping functionally related
    UI commands (`*Commands`). Shared command protocols live in
    `//ios/chrome/browser/shared/public/commands/`.
*   **Coordinator:** A controller-layer object that encapsulates a UI feature,
    manages the lifecycle of its mediator(s), view controller(s), and child
    coordinators, and configures command dispatch.
*   **Disintermediated Dispatch:** A structural pattern that connects message
    senders and receivers at runtime via a central dispatcher without coupling
    them to one another or relaying messages through an object hierarchy.
*   **Dispatcher (`CommandDispatcher`):** The `Browser`-scoped Objective-C
    object that implements disintermediated dispatch in iOS Chrome by forwarding
    registered selectors to runtime-configured target objects.
*   **Local Command Protocol:** A command protocol used strictly within a single
    feature, defined inside that feature's directory and passed directly as a
    handler without using `CommandDispatcher`.
*   **UI Command:** A `void`-returning Objective-C method defined in a command
    protocol and used as part of the UI layer's public API to request a UI
    change.
