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

 1. `static get is() {...}` holds the DOM name of the custom element.
 2. `interface HTMLElementTagNameMap {...}` informs the TypeScript compiler
     about the association between the DOM name and the class name, allowing it
     to infer the type in `document.createElement` or `querySelector`,
     `querySelectorAll` calls. Must be placed after the class definition in the
     same file.
 3. `customElements.define(...)` registers the custom element at runtime, so
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
