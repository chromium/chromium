# WebUI Lit Style Guide

[TOC]

## Overview

This style guide outlines coding standards and best practices for Chromium WebUI
development with Lit. These guidelines are intended to ensure code quality,
consistency, and performance across the codebase. They supplement the
[Chromium Web Development Style Guide](../../styleguide/web/web.md).

## CrLitElement Subclass Guidelines (.ts)

A Lit element class definition should have the following structure.

```ts
// my_button.ts example contents

import {getCss} from './my_button.css.js';
import {getHtml} from './my_button.html.js';

class MyButtonElement extends CrLitElement {
 static get is() {
    return 'my-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ...
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-button': MyButtonElement;
  }
}

customElements.define(MyButtonElement.is, MyButtonElement);
```

Specifically the following pieces are required:

 1. The class name should end with the `Element` suffix, which matches the
    naming of native HTML elements (for example `HTMLButtonElement`), and
    clearly conveys that a class is a UI component.
 2. `static get is() {...}` holds the DOM name of the custom element.
 3. `interface HTMLElementTagNameMap {...}` informs the TypeScript compiler
     about the association between the DOM name and the class name, allowing it
     to infer the type in `document.createElement` or `querySelector`,
     `querySelectorAll` calls. Must be placed after the class definition in the
     same file.
 4. `customElements.define(...)` registers the custom element at runtime, so
     that the browser knows which class to instantiate when encountering the
     corresponding DOM name. Must be placed after the class definition in the
     same file.

### Method definition order

Methods (if applicable) must be defined in the following order for consistency:
`is`, `styles`, `render`, `properties`, `constructor`, `connectedCallback`,
`disconnectedCallback`, `willUpdate`, `firstUpdated`, `updated`.

### Class fields definition order

Class fields should be defined after the `properties` getter and before the
`constructor` or, if no constructor exists, before any subsequent methods in the
method definition order.

Moreover, separate class fields that have a corresponding Lit reactive property
from class fields that don't correspond to any property with a blank
line. For example:

```ts
  static override get properties() {
    return {
      isActive: {type: Boolean},
      isDefault: {type: Boolean},
    };
  }

  accessor isActive: boolean = false;
  accessor isDefault: boolean = false;

  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();
```

### Leverage helper methods of the CrLitElement superclass.

#### Use `this.fire(...)` for firing events where possible.

**Do not:**
```ts
this.dispatchEvent(new CustomEvent(
  'event-one', {bubbles: true, composed: true}));

this.dispatchEvent(new CustomEvent(
  'event-two', {bubbles: true, composed: true, detail: someValue}));
```

**Do:**
```ts
this.fire('event-one');
this.fire('event-two', someValue);

// OK to use dispatchEvent() when firing non-bubbling or non-composed events.
this.dispatchEvent(new CustomEvent('event-three', {detail: someValue}));
```

*Note*: When a non-bubbling or non-composed event should be fired use
`dispatchEvent()` directly, as `fire()` creates bubbling and composed events.

## Template Guidelines (.html.ts)

### Logic-free Templates

Templates should remain declarative and logic-free. Avoid all of the following
in the `.html.ts` file:

* Local variable declarations like `let` or `const`.
* Function definitions, other than a single `getHtml()` function.
* if/for statements and other complex logic.

Instead:

* Delegate complex logic to helper methods defined in the component's logic file
  (`.ts`).
* If the same chunk of HTML template is needed in many places or is logically
  a mostly self-contained "module", make it a separate custom element.

The overall goal is to separate the HTML template from the element's business
logic as much as possible, and draw a clear boundary between the
responsibilities of each file. `.html.ts` files should mostly look like regular
HTML code with a bit of Lit extras (Lit expressions), and any non-trivial TS
logic should be delegated to helper methods in the .ts file.

**Do not:**
```ts
// my_element.html.ts
export function getHtml(this: MyElement) {
  const isVisible = this.someCondition && this.otherCondition;
  return html`
    <div ?hidden="${isVisible}">...</div>
  `;
}
```

**Do:**
```ts
// my_element.ts
protected accessor isVisible = false;

// my_element.html.ts
export function getHtml(this: MyElement) {
  return html`
    <div ?hidden="${!this.isVisible_}">...</div>
  `;
}
```

#### Reusing chunks of HTML
In most cases, a chunk of HTML that is needed across multiple locations in
the DOM should be refactored into a custom element that can be used as needed.
Custom elements are the canonical way of creating reusable chunks of template,
just as shared styles are the way to share CSS between elements and mixins or
helper methods/classes can be used to share TypeScript logic/behavior.

However, there are some cases where a different solution may be preferred.

1. Trivial chunks of HTML can be duplicated. Refactoring to a custom element is
   overkill for very small bits of template.

**Example:**
```ts
// my_element.html.ts
export function getHtml(this: MyElement) {
  // Small amount of repeated HTML for the cr-button is okay.
  return html`
    ${this.submitButtonFirst_ ? html`
      <cr-button id="submit" @click="${this.onSubmitClick_}">
        $i18n{submit}
      </cr-button>
    ` : ''}
    <cr-button id="cancel" @click="${this.onCancelClick_}">
      $i18n{cancel}
    </cr-button>
    ${!this.submitButtonFirst_ ? html`
      <cr-button id="submit" @click="${this.onSubmitClick_}">
        $i18n{submit}
      </cr-button>
    ` : ''}
  `;
}
```

2. Prefer conditional styling to conditional DOM changes for cases of
   modifying appearance. In some cases, it may look like a chunk of HTML
   needs to go in 2 different places in the DOM, but the only reason for
   this is to change its appearance (or the appearance of some DOM around
   it). In this case, conditional CSS styling is a better approach.

**Do not:**
```css
/* my_element.css */
.fancy-css {
  border: 4px solid blue;
}
```

```ts
// my_element.html.ts
export function getHtml(this: MyElement) {
  const button = html`
      <cr-button id="submit" @click="${this.onSubmitClick_}">
        $i18n{submit}
      </cr-button>`;
  return html`
    ${this.fancyStyleEnabled ? html`
      <div class="fancy-css">${button}</div>
    ` : button}
  `;
}
```

**Do:**
```css
/* my_element.css */
:host([fancy-style-enabled]) .wrapper {
  border: 4px solid blue;
}
```

```ts
// my_element.html.ts
export function getHtml(this: MyElement) {
  return html`
    <div class="wrapper">
      <cr-button id="submit" @click="${this.onSubmitClick_}">
        $i18n{submit}
      </cr-button>
    </div>
  `;
}
```

3. Long chunks of reused HTML that contain very few elements but a lot of
   bindings can be placed in their own helper .html.ts file, and imported.
   Cases like this would not benefit from refactoring into a custom element,
   because the repeated HTML being refactored is a long list of data bindings
   and event handlers that would simply be re-created for a new custom element.

**Example:**
```ts
// my_element_fancy_button.html.ts
export function getHtml(this: MyElement) {
  return html`
<fancy-button .prop1="${this.prop1}"
    .prop2="${this.prop2}"
    .prop3="${this.prop3}"
    .prop4="${this.prop4}"
    .prop5="${this.prop5}"
    .prop6="${this.prop6}"
    .prop7="${this.prop7}"
    .prop8="${this.prop8}"
    .prop9="${this.prop9}"
    @one="${this.onButtonOne_}"
    @two="${this.onButtonTwo_}"
    @three="${this.onButtonThree_}"
    @four="${this.onButtonFour_}">
</fancy-button>
`;
}
```

```ts
// my_element.html.ts
import {getHtml as getFancyButtonHtml} from './my_element_fancy_button.html.js';

export function getHtml(this: MyElement) {
  return html`
${this.compact_ ? html`
  <div class="compact">
    ${getFancyButtonHtml.bind(this)()}
    <cr-input id="input></cr-input>
  </div>
` : html`
  <div class="tall">
    <cr-textarea id="input></cr-textarea>
    <fancy-menu>
      ${getFancyButtonHtml.bind(this)()}
    </fancy-menu>
  </div>
`}
```

### Inline Lambdas

Do not use inline arrow functions (lambdas) in templates to pass data to event
handlers. Creating new functions on every render hurts performance and
readability.

A common reason to reach for lambdas is to bind additional data to an event
handler. Instead, bind a unique identifier, commonly an index, to a `data-*`
attribute on the element and retrieve it in the handler.


**Do not:**
```html
${this.items.map(item => html`
  <button @click="${() => this.onItemClick_(item)}">${item}</button>
`)}
```

**Do:**
```html

${this.items.map((item, index) => html`
  <button data-index="${index}" @click="${this.onItemClick_}">${item}</button>
`)}
```

```ts
protected onItemClick_(e: Event) {
  const currentTarget = e.currentTarget as HTMLElement;
  const item = items[Number(currentTarget.dataset['index'])];
}
```

### Loop Variables

When using `map` to render lists, prefer using `"item"` as the loop variable
name for consistency. Nested loops may use specific names to avoid confusion.

```ts
${this.items.map(item => html`...`)}
```

### Formatting

Wrap the return statement of your `getHtml` function or `render` method in `//
clang-format off` and `// clang-format on` comments. This prevents
`clang-format` from mangling the HTML template string, ensuring it remains
readable.

```ts
export function getHtml(this: MyCrLitElement) {
  // clang-format off
  return html`
    <!--_html_template_start_-->
    <div>...</div>
    <!--_html_template_end_-->`;
  // clang-format on
}
```

## Naming Conventions

### Events

Event names should be in lower kebab case.

Event names should state what happened, not what will happen by whoever handles
the event.

*   **Good:** `close-click`, `selection-change-click`
*   **Bad:** `close-dialog`, `update-selection`

### Event Handlers

Event handlers should be named using the pattern `on<OptionalContext>[Event]`.

*   **Good:** `onRetryClick`, `onSelectionChanged`, `onPointerdown`
*   **Bad:** `onRetryClicked`, `handleRetry`, `selectionHandler`

If the DOM node that the event handler is bound to has an ID, the
`<OptionalContext>` portion of the handler name should be that ID.

**Do not:**
```html
<button id="foo" @click="${this.onBarClick_}"></button>
```

**Do:**
```html
<button id="foo" @click="${this.onClick_}"></button>
<button id="foo" @click="${this.onFooClick_}"></button>
```

## DOM Access

### Static Elements

For elements that always exist in the Shadow DOM (not part of any conditional
rendering), prefer `this.$.<id>` to access them. This provides a strictly typed
and consistent way to access elements compared to
`this.shadowRoot.querySelector`.

**Do not:**
```ts
const button = this.shadowRoot.querySelector('#submitButton');
```

**Do:**
```ts
// In your class definition
export interface MyElement {
  $: {
    submitButton: CrButtonElement,
  }
}

// In your code
const button = this.$.submitButton;
```
