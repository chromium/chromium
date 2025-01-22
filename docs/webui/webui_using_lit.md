# Using Lit in Chromium WebUI Development

[TOC]

## Background

This documentation focuses on using Lit in the context of Chromium WebUI,
and on compatibility between Lit and Polymer elements, since much of Chromium
WebUI is currently written in Polymer. It assumes familiarity with the
following:

*   Web Components: The basic foundation on which Lit is built. See
    [Introduction to Web Components from MDN](https://developer.mozilla.org/en-US/docs/Web/API/Web_components) and the related
    [MDN guide for custom elements](https://developer.mozilla.org/en-US/docs/Web/API/Web_components/Using_custom_elements).
*   Lit framework, see [official docs](https://lit.dev/docs/), and especially
    the [Reactive properties, Lifecycle](https://lit.dev/docs/components/lifecycle/)
    and [Expressions](https://lit.dev/docs/templates/expressions/) sections.
*   Lit vs Polymer differences. See
    [Lit for Polymer users](https://lit.dev/articles/lit-for-polymer-users/),
    covering some general Lit/Polymer compatibility and migration issues

***promo
For developers unfamiliar with these, the external sources linked above are
recommended before continuing with this documentation.
***

## Chromium WebUI Infrastructure: CrLitElement base class
<code>[CrLitElement](https://source.chromium.org/chromium/chromium/src/+/main:third_party/lit/v3_0/cr_lit_element.ts;l=1?q=cr_lit_element&sq=&ss=chromium%2Fchromium%2Fsrc)</code> is provided as a base class for Chromium WebUI development. It contains code to

1. Reduce the amount of boilerplate code necessary for individual elements.
2. Improve compatibility with elements using Polymer (necessary in a codebase
   that is using both Polymer and Lit).
3. Make Polymer -> Lit migrations easier

***promo
Lit custom elements in Chromium should inherit from the `CrLitElement` class.
***

Specific features of `CrLitElement` include:

1. Forces initial rendering to be synchronous when
    1. The element’s connectedCallback runs
    2. The element is focused before the connectedCallback has run
    3. Child elements are accessed with `this.$` before the connectedCallback
       has run

    This means that there is no need to call `await element.updateComplete`
    before accessing the element’s DOM in certain cases:
    1. Immediately after attaching it
    2. When doing something like focusing the element or accessing its children
       with `element.$` in a Polymer parent’s `connectedCallback()`, which may
       run before the Lit child’s `connectedCallback()`.

    For more detail on this, see the inline documentation in `CrLitElement`.
2. “$” proxy to allow accessing child elements by ID with `this.$.id`, to match
   the behavior of Polymer, which does the same thing.
3. Implementation of `notify: true` for properties. Setting this will cause
   `CrLitElement` to fire `foo-changed` events in the `updated()` lifecycle
   callback, whenever some property `foo` with `notify: true` set is changed.
   This allows compatibility with Polymer parent element two-way bindings, but
   is not exactly the same; see more details below.
4. Changes Lit’s default property/attribute mapping to match the mapping used
   in Polymer - i.e. property `fooBar` will be mapped to attribute `foo-bar`,
   not the Lit default of `foobar`

## Lit Data Bindings and handling `-changed` events
As noted above, `CrLitElement` forces an initial synchronous render in
`connectedCallback()`. This means child elements may initialize properties
with `notify: true` and then fire `-changed` events for these properties in
`updated()` as soon as they are connected, which may occur before the parent
element has finished its first update.

One consequence of this is that `-changed` event handlers cannot assume that
the element has completed its first update when the `-changed` event is
received, and should not make any changes to the element's DOM until after
waiting for the element's `updateComplete` promise. This means such handlers
must either (1) be async and `await this.updateComplete;` before running any
code that updates the element's DOM, or (2) only update properties on the
parent in response to the child's property change, and perform resulting UI
updates in the `updated()` lifecycle method instead.

Note that if the parent property being updated is protected or private, a cast
will be necessary to check for changes to the property in `changedProperties`.
This is also demonstrated in the example below.

Suppose the Lit child has a property with `notify: true` as follows:
```
static override get properties() {
  return {
    foo: {
      type: Boolean,
      notify: true,
    },
 };
}
```

This property is also bound to a parent element that listens for the
`-changed` event as follows:
```
<foo-child ?foo="${this.foo_}" on-foo-changed="${this.onFooChanged_}">
</foo-child>
<demo-child id="demo"></demo-child>
```

The parent TypeScript code could look like this:
```
static override get properties() {
  return {
    foo_: {type: Boolean},
 };
}

protected foo_: boolean = true;

onFooChanged_(e: CustomEvent<{value: boolean}>) {
  // Updates the parent's property that is bound to the child.
  this.foo_ = e.detail.value;
}

override updated(changedProperties: PropertyValues<this>) {
  super.updated(changedProperties);

  // Cast necessary to check for changes to protected/private properties.
  const changedPrivateProperties =
      changedProperties as Map<PropertyKey, unknown>;

  // Updates the DOM when |foo_| changes.
  if (changedPrivateProperties.has('foo_')) {
    if (this.foo_) {
      this.$.demo.show();
    } else {
      this.$.demo.hide();
    }
  }
}
```

## Lit data binding issue with select elements
The `<select>` element has an ordering requirement that sometimes causes a
bug when using Lit data bindings on both the `value` property of the
`<select>` and the `value` attribute of its child `<option>` elements.
Specifically, when the `<select>`'s `value` property is set, there must
already be an existing `<option>` with that value, or the `<select>` will
be rendered as blank. If Lit bindings are used for the `<option>` values,
these values will not be populated in time, and the `<select>` will be
empty at startup. The following example would reproduce this bug and
have an empty `<select>` displayed at startup.

`.html.ts` file with `<select>` bug:
```
<select .value="${this.mySelectValue}" @change="${this.onSelectChange_}">
  <option value="${MyEnum.FIRST}">Option 1</option>
  <option value="${MyEnum.SECOND}">Option 2</option>
</select>
```

Corresponding `.ts`. Note that the bug manifests even though `mySelectValue`
is being initialized to a valid option.
```
static get properties() {
  return {
    mySelectValue: {type: String},
  };
}

mySelectValue: MyEnum = MyEnum.SECOND;

onSelectChange_(e: Event) {
  this.mySelectValue = (e.target as HTMLSelectElement).value;
}
```

The current recommended workaround is to instead bind to the `selected`
attribute on each `<option>`, i.e.:

`.html.ts` file:
```
<select @change="${this.onSelectChange_}">
  <option value="${MyEnum.FIRST}"
      ?selected="${this.isSelected_(MyEnum.FIRST)}">
    Option 1
  </option>
  <option value="${MyEnum.SECOND}"
      ?selected="${this.isSelected_(MyEnum.SECOND)}">
    Option 2
  </option>
</select>
```

Corresponding `.ts` file:
```
static get properties() {
  return {
    mySelectValue: {type: String},
  };
}

mySelectValue: MyEnum = MyEnum.SECOND;

onSelectChange_(e: Event) {
  this.mySelectValue = (e.target as HTMLSelectElement).value;
}

isSelected_(value: MyEnum): boolean {
  return value === this.mySelectValue;
}
```

Note: This bug can also be worked around by using Lit's `live` directive in
the data binding and requesting an extra update any time the `<select>` is
rendered. Including `live` in the Chromium Lit bundle is still under
consideration. Reach out to the WebUI team if you have a `<select>` where the
workaround above is problematic or impractical (e.g. due to a huge list of
`<option>` elements).

## Lit and Polymer Data Bindings Compatibility
Two-way bindings are not natively supported in Lit. As mentioned above,
basic compatibility is provided by the `CrLitElement` base class’s
implementation of `notify: true`. However, these events differ from the Polymer
two-way binding behavior.
* In Polymer two-way bindings, the child element only fires a `-changed`
  event if the property is modified from the child. The child element does not
  fire the event if the property value is set from the parent.
* The equivalent code in `CrLitElement` can’t differentiate  whether the
  property was set from the parent or the child itself and always fires the
  `-changed` event when the value changes.

*** promo
When migrating parent or child elements to Lit, any code directly handling
`-changed` events in the parent should either be agnostic to whether the
corresponding property was changed from the parent or the child, or should check
that the new value in the event differs from the current parent value.
***

Example: a `-changed` event handler that logs a metric indicating something was
changed from the child (e.g. due to user input) should add a check before
logging that the new value is in fact new, and was not set by the parent via the
data binding.

The behavior difference for two-way bindings can be seen by playing with this [Lit playground example](https://lit.dev/playground/#project=W3sibmFtZSI6InRvcC1lbGVtZW50LnRzIiwiY29udGVudCI6ImltcG9ydCB7UG9seW1lckVsZW1lbnQsIGh0bWx9IGZyb20gJ0Bwb2x5bWVyL3BvbHltZXInO1xuaW1wb3J0IHtjdXN0b21FbGVtZW50LCBwcm9wZXJ0eX0gZnJvbSAnQHBvbHltZXIvZGVjb3JhdG9ycyc7XG5pbXBvcnQge0NyRHVtbXlQb2x5bWVyRWxlbWVudH0gZnJvbSAnLi9jcl9kdW1teV9wb2x5bWVyLmpzJztcbmltcG9ydCB7Q3JEdW1teUxpdEVsZW1lbnR9IGZyb20gJy4vY3JfZHVtbXlfbGl0LmpzJztcbmltcG9ydCAnLi9jcl9kdW1teV9saXQuanMnO1xuaW1wb3J0ICcuL2NyX2R1bW15X3BvbHltZXIuanMnO1xuXG5AY3VzdG9tRWxlbWVudCgndG9wLWVsZW1lbnQnKVxuY2xhc3MgVG9wRWxlbWVudCBleHRlbmRzIFBvbHltZXJFbGVtZW50IHtcbiAgc3RhdGljIGdldCB0ZW1wbGF0ZSgpIHtcbiAgICByZXR1cm4gaHRtbGBcbiAgICAgIDxzdHlsZT5cbiAgICAgICAgOmhvc3Qge1xuICAgICAgICAgIGNvbG9yOiBibHVlO1xuICAgICAgICAgIGRpc3BsYXk6YmxvY2s7XG4gICAgICAgICAgYm9yZGVyOiAycHggc29saWQgYmx1ZTtcbiAgICAgICAgfVxuICAgICAgICBcbiAgICAgICAgI2NoaWxkcmVuIHtcbiAgICAgICAgICBkaXNwbGF5OiBmbGV4O1xuICAgICAgICAgIGdhcDogMTBweDtcbiAgICAgICAgfVxuICAgICAgPC9zdHlsZT5cbiAgICAgIDxkaXY-UE9MWU1FUiBQQVJFTlQ8L2Rpdj5cbiAgICAgIDxsYWJlbD5TZXQgUG9seW1lciBwYXJlbnQgdmFsdWU6IDxpbnB1dCB0eXBlPVwibnVtYmVyXCIgb24taW5wdXQ9XCJvbklucHV0X1wiPjwvaW5wdXQ-PC9sYWJlbD5cbiAgICAgIDxkaXYgaWQ9XCJjaGlsZHJlblwiPlxuICAgICAgPGNyLWR1bW15LXBvbHltZXIgdmFsdWU9XCJ7e3ZhbHVlUG9seW1lcn19XCIgb24tdmFsdWUtY2hhbmdlZD1cIm9uUG9seW1lclZhbHVlQ2hhbmdlZF9cIj48L2NyLWR1bW15LXBvbHltZXI-XG4gICAgICA8Y3ItZHVtbXktbGl0IHZhbHVlPVwie3t2YWx1ZUxpdH19XCIgb24tdmFsdWUtY2hhbmdlZD1cIm9uTGl0VmFsdWVDaGFuZ2VkX1wiPjwvY3ItZHVtbXktbGl0PlxuICAgICAgPC9kaXY-XG4gICAgICA8ZGl2IGlkPVwibG9nXCI-PC9kaXY-XG4gICAgYDtcbiAgfVxuICBcbiAgc3RhdGljIGdldCBwcm9wZXJ0aWVzKCkge1xuICAgIHJldHVybiB7XG4gICAgICB2YWx1ZVBvbHltZXI6IHt0eXBlOiBOdW1iZXJ9LFxuICAgICAgdmFsdWVMaXQ6IHt0eXBlOiBOdW1iZXJ9LFxuICAgIH07XG4gIH1cbiAgXG4gIHZhbHVlTGl0OiBudW1iZXI7XG4gIHZhbHVlUG9seW1lcjogbnVtYmVyO1xuICBcbiAgb25JbnB1dF8oKSB7ICAgIFxuICAgIHRoaXMudmFsdWVQb2x5bWVyID0gTnVtYmVyKHRoaXMuc2hhZG93Um9vdC5xdWVyeVNlbGVjdG9yKCdpbnB1dCcpLnZhbHVlKTtcbiAgICB0aGlzLnZhbHVlTGl0ID0gdGhpcy52YWx1ZVBvbHltZXI7XG4gIH1cbiAgXG4gIG9uUG9seW1lclZhbHVlQ2hhbmdlZF8oZSkge1xuICAgIGNvbnN0IGR1bW15ID0gdGhpcy5zaGFkb3dSb290IS5xdWVyeVNlbGVjdG9yKCdjci1kdW1teS1wb2x5bWVyJykgYXMgQ3JEdW1teVBvbHltZXJFbGVtZW50O1xuICAgIHRoaXMuJC5sb2cudGV4dENvbnRlbnQgKz0gJ3BvbHltZXItdmFsdWUtY2hhbmdlZCB0byAnICsgZS5kZXRhaWwudmFsdWUgKyAnLi4uICAnO1xuICB9XG4gICBvbkxpdFZhbHVlQ2hhbmdlZF8oZSkge1xuICAgIGNvbnN0IGR1bW15ID0gdGhpcy5zaGFkb3dSb290IS5xdWVyeVNlbGVjdG9yKCdjci1kdW1teS1saXQnKSBhcyBDckR1bW15TGl0RWxlbWVudDtcbiAgICB0aGlzLiQubG9nLnRleHRDb250ZW50ICs9ICdsaXQtdmFsdWUtY2hhbmdlZCB0byAnICsgZS5kZXRhaWwudmFsdWUgKyAnLi4uICAnO1xuICB9XG59O1xuXG4ifSx7Im5hbWUiOiJwYWNrYWdlLmpzb24iLCJjb250ZW50Ijoie1xuICBcImRlcGVuZGVuY2llc1wiOiB7XG4gICAgXCJsaXRcIjogXCJeMy4wLjBcIixcbiAgICBcIkBsaXQvcmVhY3RpdmUtZWxlbWVudFwiOiBcIl4yLjAuMFwiLFxuICAgIFwibGl0LWVsZW1lbnRcIjogXCJeNC4wLjBcIixcbiAgICBcImxpdC1odG1sXCI6IFwiXjMuMC4wXCJcbiAgfVxufSIsImhpZGRlbiI6dHJ1ZX0seyJuYW1lIjoiaW5kZXguaHRtbCIsImNvbnRlbnQiOiI8IURPQ1RZUEUgaHRtbD5cbjxoZWFkPlxuICA8c2NyaXB0IHR5cGU9XCJtb2R1bGVcIiBzcmM9XCIuL3RvcC1lbGVtZW50LmpzXCI-PC9zY3JpcHQ-XG4gIDxzY3JpcHQgdHlwZT1cIm1vZHVsZVwiIHNyYz1cIi4vdG9wLWVsZW1lbnQtbGl0LmpzXCI-PC9zY3JpcHQ-XG48L2hlYWQ-XG48Ym9keT5cbiAgPHRvcC1lbGVtZW50PjwvdG9wLWVsZW1lbnQ-XG4gIDx0b3AtZWxlbWVudC1saXQ-PC90b3AtZWxlbWVudC1saXQ-XG48L2JvZHk-XG4ifSx7Im5hbWUiOiJjcl9kdW1teV9wb2x5bWVyLnRzIiwiY29udGVudCI6ImltcG9ydCB7UG9seW1lckVsZW1lbnQsIGh0bWx9IGZyb20gJ0Bwb2x5bWVyL3BvbHltZXInO1xuaW1wb3J0IHtjdXN0b21FbGVtZW50LCBwcm9wZXJ0eX0gZnJvbSAnQHBvbHltZXIvZGVjb3JhdG9ycyc7XG5cbkBjdXN0b21FbGVtZW50KCdjci1kdW1teS1wb2x5bWVyJylcbmV4cG9ydCBjbGFzcyBDckR1bW15UG9seW1lckVsZW1lbnQgZXh0ZW5kcyBQb2x5bWVyRWxlbWVudCB7XG4gIHN0YXRpYyBnZXQgdGVtcGxhdGUoKSB7XG4gICAgcmV0dXJuIGh0bWxgXG4gICAgICAgPHN0eWxlPlxuICAgICAgICA6aG9zdCB7XG4gICAgICAgICAgYm9yZGVyOiAycHggc29saWQgYmx1ZTtcbiAgICAgICAgICBjb2xvcjogYmx1ZTtcbiAgICAgICAgICBkaXNwbGF5OiBibG9jaztcbiAgICAgICAgfVxuICAgICAgIDwvc3R5bGU-XG4gICAgICAgPGRpdj5Qb2x5bWVyIGNoaWxkIHZhbHVlIGlzOiBbW3ZhbHVlXV08L2Rpdj5cbiAgICBgO1xuICB9XG4gIFxuICBzdGF0aWMgZ2V0IHByb3BlcnRpZXMoKSB7XG4gICAgcmV0dXJuIHtcbiAgICAgIHZhbHVlOiB7dHlwZTogTnVtYmVyLCBub3RpZnk6IHRydWV9LFxuICAgIH07XG4gIH1cbiAgXG4gIHZhbHVlOiBudW1iZXIgPSAxO1xufSJ9LHsibmFtZSI6InRvcC1lbGVtZW50LWxpdC50cyIsImNvbnRlbnQiOiJpbXBvcnQge0xpdEVsZW1lbnQsIGh0bWwsIG5vdGhpbmd9IGZyb20gJ2xpdCc7XG5pbXBvcnQge2N1c3RvbUVsZW1lbnR9IGZyb20gJ2xpdC9kZWNvcmF0b3JzLmpzJztcblxuaW1wb3J0IHtDckR1bW15UG9seW1lckVsZW1lbnR9IGZyb20gJy4vY3JfZHVtbXlfcG9seW1lci5qcyc7XG5pbXBvcnQgJy4vY3JfZHVtbXlfcG9seW1lci5qcyc7XG5pbXBvcnQge0NyRHVtbXlMaXRFbGVtZW50fSBmcm9tICcuL2NyX2R1bW15X2xpdC5qcyc7XG5pbXBvcnQgJy4vY3JfZHVtbXlfbGl0LmpzJztcblxuQGN1c3RvbUVsZW1lbnQoJ3RvcC1lbGVtZW50LWxpdCcpXG5jbGFzcyBUb3BFbGVtZW50TGl0IGV4dGVuZHMgTGl0RWxlbWVudCB7XG4gIG92ZXJyaWRlIHJlbmRlcigpIHtcbiAgICByZXR1cm4gaHRtbGBcbiAgICAgIDxzdHlsZT5cbiAgICAgICAgOmhvc3Qge1xuICAgICAgICAgIGJvcmRlcjogMnB4IHNvbGlkIGdyZWVuO1xuICAgICAgICAgIGNvbG9yOiBncmVlbjtcbiAgICAgICAgICBkaXNwbGF5OiBibG9jaztcbiAgICAgICAgfVxuICAgICAgICBcbiAgICAgICAgI2NoaWxkcmVuIHtcbiAgICAgICAgICBkaXNwbGF5OiBmbGV4O1xuICAgICAgICAgIGdhcDogMTBweDtcbiAgICAgICAgfVxuICAgICAgPC9zdHlsZT5cbiAgICAgIDxkaXY-TElUIFBBUkVOVDwvZGl2PlxuICAgICAgPGxhYmVsPlNldCBMaXQgcGFyZW50IHZhbHVlOiA8aW5wdXQgdHlwZT1cIm51bWJlclwiIEBpbnB1dD1cIiR7dGhpcy5vbklucHV0X31cIj48L2lucHV0PjwvbGFiZWw-XG4gICAgICA8ZGl2IGlkPVwiY2hpbGRyZW5cIj5cbiAgICAgIDxjci1kdW1teS1wb2x5bWVyIHZhbHVlPVwiJHt0aGlzLnZhbHVlUG9seW1lciB8fCBub3RoaW5nfVwiIEB2YWx1ZS1jaGFuZ2VkPVwiJHt0aGlzLm9uUG9seW1lclZhbHVlQ2hhbmdlZF99XCI-PC9jci1kdW1teS1wb2x5bWVyPlxuICAgICAgPGNyLWR1bW15LWxpdCB2YWx1ZT1cIiR7dGhpcy52YWx1ZUxpdCB8fCBub3RoaW5nfVwiIEB2YWx1ZS1jaGFuZ2VkPVwiJHt0aGlzLm9uTGl0VmFsdWVDaGFuZ2VkX31cIj48L2NyLWR1bW15LWxpdD5cbiAgICAgIDwvZGl2PlxuICAgICAgPGRpdiBpZD1cImxvZ1wiPjwvZGl2PlxuICAgIGA7XG4gIH1cblxuICBzdGF0aWMgb3ZlcnJpZGUgZ2V0IHByb3BlcnRpZXMoKSB7XG4gICAgcmV0dXJuIHtcbiAgICAgIHZhbHVlUG9seW1lcjoge3R5cGU6IE51bWJlcn0sXG4gICAgICB2YWx1ZUxpdDoge3R5cGU6IE51bWJlcn0sXG4gICAgfTtcbiAgfVxuICBcbiAgdmFsdWVQb2x5bWVyOiBudW1iZXI7XG4gIHZhbHVlTGl0OiBudW1iZXI7XG4gIFxuICBvbklucHV0XygpIHtcbiAgICB0aGlzLnZhbHVlUG9seW1lciA9IE51bWJlcih0aGlzLnNoYWRvd1Jvb3QucXVlcnlTZWxlY3RvcignaW5wdXQnKS52YWx1ZSk7XG4gICAgdGhpcy52YWx1ZUxpdCA9IHRoaXMudmFsdWVQb2x5bWVyO1xuICB9XG4gIFxuICBvblBvbHltZXJWYWx1ZUNoYW5nZWRfKGUpIHtcbiAgICBjb25zdCBkdW1teSA9IHRoaXMuc2hhZG93Um9vdCEucXVlcnlTZWxlY3RvcignY3ItZHVtbXktcG9seW1lcicpIGFzIENyRHVtbXlQb2x5bWVyRWxlbWVudDtcbiAgICB0aGlzLnNoYWRvd1Jvb3QhLnF1ZXJ5U2VsZWN0b3IoJyNsb2cnKS50ZXh0Q29udGVudCArPSAncG9seW1lci12YWx1ZS1jaGFuZ2VkIHRvICcgKyBlLmRldGFpbC52YWx1ZSArICcuLi4gICc7XG4gIH1cbiAgXG4gIG9uTGl0VmFsdWVDaGFuZ2VkXyhlKSB7XG4gICAgY29uc3QgZHVtbXkgPSB0aGlzLnNoYWRvd1Jvb3QhLnF1ZXJ5U2VsZWN0b3IoJ2NyLWR1bW15LWxpdCcpIGFzIENyRHVtbXlMaXRFbGVtZW50O1xuICAgIHRoaXMuc2hhZG93Um9vdCEucXVlcnlTZWxlY3RvcignI2xvZycpLnRleHRDb250ZW50ICs9ICdsaXQtdmFsdWUtY2hhbmdlZCB0byAnICsgZS5kZXRhaWwudmFsdWUgKyAnLi4uICAnO1xuICB9XG59XG4gIn0seyJuYW1lIjoiY3JfZHVtbXlfbGl0LnRzIiwiY29udGVudCI6ImltcG9ydCB7TGl0RWxlbWVudCwgaHRtbCwgUHJvcGVydHlWYWx1ZXN9IGZyb20gJ2xpdCc7XG5pbXBvcnQge2N1c3RvbUVsZW1lbnR9IGZyb20gJ2xpdC9kZWNvcmF0b3JzLmpzJztcblxuQGN1c3RvbUVsZW1lbnQoJ2NyLWR1bW15LWxpdCcpXG5leHBvcnQgY2xhc3MgQ3JEdW1teUxpdEVsZW1lbnQgZXh0ZW5kcyBMaXRFbGVtZW50IHtcbiAgb3ZlcnJpZGUgcmVuZGVyKCkge1xuICAgIHJldHVybiBodG1sYFxuICAgICAgPHN0eWxlPlxuICAgICAgICA6aG9zdCB7XG4gICAgICAgICAgYm9yZGVyOiAycHggc29saWQgZ3JlZW47XG4gICAgICAgICAgY29sb3I6IGdyZWVuO1xuICAgICAgICAgIGRpc3BsYXk6IGJsb2NrO1xuICAgICAgICB9XG4gICAgICA8L3N0eWxlPlxuICAgICAgPGRpdj5MaXQgY2hpbGQgdmFsdWUgaXM6ICR7dGhpcy52YWx1ZX08L2Rpdj5cbiAgICBgO1xuICB9XG5cbiAgc3RhdGljIG92ZXJyaWRlIGdldCBwcm9wZXJ0aWVzKCkge1xuICAgIHJldHVybiB7XG4gICAgICB2YWx1ZToge3R5cGU6IE51bWJlcn0sXG4gICAgfTtcbiAgfVxuICBcbiAgb3ZlcnJpZGUgY29ubmVjdGVkQ2FsbGJhY2soKSB7XG4gICAgc3VwZXIuY29ubmVjdGVkQ2FsbGJhY2soKTtcbiAgICBcbiAgICB0aGlzLnBlcmZvcm1VcGRhdGUoKTtcbiAgfVxuIFxuICB2YWx1ZTogbnVtYmVyID0gMTtcbiAgXG4gIC8vIEltcGxlbWVudGF0aW9uIG9mIG5vdGlmeTogdHJ1ZSBmcm9tIENyTGl0RWxlbWVudFxuICBvdmVycmlkZSB1cGRhdGVkKGNoYW5nZWRQcm9wZXJ0aWVzOiBQcm9wZXJ0eVZhbHVlczx0aGlzPikge1xuICAgIGlmIChjaGFuZ2VkUHJvcGVydGllcy5oYXMoJ3ZhbHVlJykpIHtcbiAgICAgIHRoaXMuZGlzcGF0Y2hFdmVudChuZXcgQ3VzdG9tRXZlbnQoJ3ZhbHVlLWNoYW5nZWQnLCB7IGJ1YmJsZXM6IHRydWUsIGNvbXBvc2VkOiB0cnVlLCBkZXRhaWw6IHsgdmFsdWU6IHRoaXMudmFsdWUgfX0pKTtcbiAgICB9XG4gIH1cbn0ifV0).
Modifying initialization of the properties in the different parent and child
elements in this playground example reveals that there are also differences
in behavior on initialization. These differences are also documented in the
following table:

| |Property initialized in both parent and child|Property initialized in parent only|Property initialized in child only|Property uninitialized in both parent adnd child|
|---|---|---|---|---|
|**Polymer parent hosting Polymer child**|Parent value propagates to child. No `-changed` event fired.|Parent value propagates to child. No `-changed` event fired.|Child value propagates to parent. `-changed` event fired.|Property is left undefined. No events fired.|
|**Polymer parent hosting Lit child**|Parent value propagates to child. `-changed` event fired.|Parent value propagates to child. `-changed` event fired.|Child value propagates to parent. `-changed` event fired.|Property is left undefined. No events fired.|
|**Lit parent hosting Polymer child**|Parent value propagates to child. `-changed` event fired.|Parent value propagates to child. `-changed` event fired.|Child value propagates to parent if binding is using attribute syntax\*. If using property syntax\*\*, parent `undefined` value takes precedence and `-changed` event fires with `undefined`.|Property is left undefined. No events fired.|
|**Lit parent hosting Lit child**|Parent value propagates to child. `-changed` event fired.|Parent value propagates to child. `-changed` event fired.| Child value propagates to parent if binding is using attribute syntax\*. If using property syntax\*\*, parent `undefined` value takes precedence and `-changed` event fires with `undefined`.|Property is left undefined. No events fired.|

***aside
\* attribute syntax: `value="${this.childValue || nothing}"`

\*\* property syntax: `.value="${this.value}" `Note that this syntax must be used for anything that is not a boolean, string, or number.
***

## Polymer iron/paper elements alternatives
Previously, code in Chromium WebUI relied heavily on the Polymer library of
elements in addition to the Polymer framework itself. The following table
captures the list of elements that were still being used in Desktop WebUI code
when the WebUI team started exploring Lit as an alternative to Polymer, and
discusses the recommended future approach for each. Some of these
elements have subsequently been removed from Desktop (non-CrOS) builds, and more
will be removed over time.

*** note
Note: `iron-` and `paper-` elements are
[no longer recommended even for Polymer UIs.](https://chromium.googlesource.com/chromium/src/+/HEAD/styleguide/web/web.md#Polymer)
***

|POLYMER LIBRARY ELEMENT|RECOMMENDED APPROACH|
|-----------------------|--------------------|
|`iron-list`|Use `cr-infinite-list` or, if the list is not very large, use the `map()` directive. See additional detail on migrating `iron-list` clients below.|
|`iron-icon`|Use `cr-icon`.|
|`iron-collapse`|Use `cr-collapse`.|
|`paper-spinner`|Use `cr-spinner-style` CSS, or for more customization style `throbber.svg` as needed.|
|`paper-styles`|Do not use, these styles are pre-2023 refresh and have been removed on non-CrOS builds.|
|`iron-flex-layout`|Do not use, use standard CSS to style elements.|
|`iron-a11y-announcer`|Use `cr-a11y-announcer`|
|`iron-location`/`iron-query-params`|Use `CrRouter` or custom code.|
|`iron-scroll-target-behavior`|Do not use.|
|`paper-progress`|Use either `cr-progress` or the native `<progress>` element with CSS styling.|
|`iron-iconset-svg`/`iron-meta`|Use `cr-iconset` and `IconsetMap`.|
|`iron-media-query`|Do not use, use `window.matchMedia()`.|
|`iron-pages`|Do not use, replace with equivalent conditional rendering or use `cr-page-selector`.|
|`iron-scroll-threshold`|Do not use.|
|`iron-resizable-behavior`|Do not use. `ResizeObserver`s can be used to trigger changes in items that need to be modified when something is resized.|
|`paper-tooltip`|Use `cr-tooltip`.|
|`iron-a11y-keys`|Do not use.|
|`iron-selector`/`iron-selectable-behavior`|Use `CrSelectableMixin`.|

## Anatomy of a Lit-based element
In Chromium WebUI, Lit based custom elements are defined using 3 files:
1. A \*`.ts` file defining the element, which imports the template from the
   \*`.html.js` file and the style from the \*`.css.js` file.
2. A \*`.css` file containing the element’s styling. This file is run through
   `css_to_wrapper` at build time to create a \*`.css.js` file that the
   element’s \*`.ts` file can import the styles from.
3. A \*`.html.ts` file containing the element’s HTML template. This file can be
   either
   *   auto-generated from a checked-in `*.html` file via `html_to_wrapper`
       (preferred approach for Polymer->Lit migrations), OR
   *   directly be checked in to the repository (preferred approach for new Lit
       code, or post migration cleanups)

***note
Note: This differs from Polymer based custom elements in Chromium, which
typically use only 2 files: a `.html` file containing both the element’s
template and its styling, and a `.ts` file containing the element definition.
***

Example `.ts` file:
```
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '//resources/cr_elements/cr_input/cr_input.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {getCss} from './my_example.css.js';
import {getHtml} from './my_example.html.js';

export interface MyExampleElement {
  $: {
    input: HTMLElement,
  };
}

export class MyExampleElement extends CrLitElement {
  static get is() {
    return 'my-example';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {
        type: Boolean,
        reflect: true,
      },
      myValue: {type: String},
    };
  }

  disabled: boolean = false;
  myValue: string = 'hello world';

  // Referenced from the template, so must be protected (not private).
  protected onInputValueChanged_(e: CustomEvent<string>) {
    this.myValue = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'my-example': MyExampleElement;
  }
}

customElements.define(MyExampleElement.is, MyExampleElement);
```

Example CSS file:
```
/* Copyright 2024 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/* #css_wrapper_metadata_start
 * #type=style-lit
 * #import=//resources/cr_elements/cr_shared_vars.css.js
 * #scheme=relative
 * #css_wrapper_metadata_end */

#input {
  background-color: blue;
  --cr-input-error-display: none;
}

:host([disabled]) #input {
   background-color: gray;
}
```

***note
CSS files holding Lit element styles should begin with metadata comments,
between 2 lines marked with `#css_wrapper_metadata_start` and
`#css_wrapper_metadata_end`. These comments tell `css_to_wrapper()` how to
generate the wrapper `.css.ts` file.
*   `style-lit` indicates this is a Lit style file
*   imports are specified with `#import=//import/path/for/file`
*   includes (not used in this particular example) are specified with
    `#include="style-name-1 style-name-2"`
*   `scheme=relative` indicates imports should be scheme-relative
    (i.e. use “`//resources`”)
***

Example `.html.ts `file:
```
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {MyExampleElement} from './my_example.js';

export function getHtml(this: MyExampleElement) {
 return html`
   <div>Input something</div>
   <cr-input id="input" .value="${this.myValue}"
       ?disabled="${this.disabled}"
       @value-changed="${this.onInputValueChanged_}">
   </cr-input>`;
}
```

`BUILD.gn` file configuration:
```
build_webui("build") {
  …
  # Use non_web_component_files since the .html.ts file is checked in.
  non_web_component_files = [
     "my_example.html.ts",
     "my_example.ts",
  ]
  # Unlike Polymer, when using Lit non-shared CSS code resides in dedicated
  # CSS files passed to css_to_wrapper.
  css_files = [
    "my_example.css",
  ]
  # Other TS Compiler related arguments…
  ts_deps = [
    "//ui/webui/resources/cr_elements:build_ts",
    "//third_party/lit/v3_0:build_ts",
  ]
}
```
***note
Note that unlike for Polymer custom elements,
both `.ts` and `.html.ts` files are passed as `non_web_component_files`. This
indicates to `build_webui()` that they do not have a corresponding `.html` file
that needs to be passed to `html_to_wrapper()` (since in the case of Lit
elements, `.html.ts` files are checked in directly).
***

## Polymer to Lit migrations

Many of the boilerplate steps for migrating from Polymer to Lit are documented
in the [readme](https://chromium.googlesource.com/chromium/src/+/main/ui/webui/resources/tools/codemods/lit_migration.md)
in the Lit migration [script](https://source.chromium.org/chromium/chromium/src/+/main:ui/webui/resources/tools/codemods/lit_migration.py)
folder.

***promo
Most of the basic boilerplate migration steps can be automated using the
migration script.
***

Prior to running the script, [jscodeshift](https://github.com/facebook/jscodeshift#readme)
needs to be downloaded and installed as follows:

```
npm install --prefix ui/webui/resources/tools/codemods jscodeshift
```

The script can be invoked to begin migrating an element from Polymer to Lit as
follows (replace the `most_visited.ts` file path with the file being migrated):

```
python3 ui/webui/resources/tools/codemods/lit_migration.py \
   --file ui/webui/resources/cr_components/most_visited/most_visited.ts
```

The rest of this section describes migration steps that cannot be automated
using the script.

### Computed properties
Computed properties in Polymer may not be needed after migrating Lit, if the
properties are simply used to populate some part of the element’s template and
are not reflected as attributes or double-bound to a Polymer parent. Lit
automatically re-renders any changed parts of the template without needing to
have the individual properties listed as parameters in the HTML template, so
in these cases the computation method can be used directly in the template
without specifying parameters. An example of this follows.

Polymer HTML template snippet:
```
<cr-button hidden="[[hideButton_]]">Click Me</cr-button>
```

In the Polymer element definition:
```
static get properties() {
  return {
   loading: Boolean,
   showingDialog: Boolean,
   hideButton_: {
     type: Boolean,
     computed: 'computeHideButton_(loading, showingDialog)',
   },
 };
}
// Other code goes here

private computeHideButton_(): boolean {
  return !this.loading && !this.showingDialog;
}
```

This could be rewritten in Lit, omitting the `hideButton_` property entirely.

Equivalent Lit HTML template snippet:
```
<cr-button ?hidden="${this.computeHideButton_()}">Click Me</cr-button>
```

Equivalent Lit element definition:
```
static get properties() {
  return {
   loading: {type: Boolean},
   showingDialog: {type: Boolean},
 };
}
// Other code goes here
// Anything referenced in the HTML template needs to be protected, not
// private.
protected computeHideButton_(): boolean {
  return !this.loading && !this.showingDialog;
}
```

In other cases, where computed properties are bound to other elements, used as
attributes, or are needed for other internal logic, they can be computed in the
`willUpdate()` lifecycle callback when the properties that they depend on change
as in the following example:
```
override willUpdate(changedProperties: PropertyValues<this>) {
  super.willUpdate(changedProperties);

  if (changedProperties.has('value')) {
    const values = (this.value || '').split(',');
    this.multipleValues_ = values.length > 1;
  }
}
```

### Observers
Observer code should be triggered in either the `willUpdate()` lifecycle
callback or the `updated()` lifecycle callback, depending on whether it is
internal logic or requires accessing the element’s DOM:
*   Any code that measures or queries the element’s DOM belongs in `updated()`
    to avoid measuring or querying before rendering has actually completed for
    the current cycle.
*   Otherwise, updates to other reactive properties should generally be put in
    `willUpdate() `so that any resulting template changes from these property
    updates can be batched with the other changes in a single update, rather
    than triggering a second round of updates.

Consider the following Polymer code, with a complex observer:
```
static get properties() {
  return {
   max: Number,
   min: Number,
   value: Number,
 };
}

static get observers() {
  return [ 'onValueSet_(min, max, value)' ];
}

private onValueSet_() {
  this.value = Math.min(Math.max(this.value, this.min), this.max);
  const demo = this.shadowRoot!.querySelector('#demo');
  if (demo) {
    demo.style.height = `${this.value}px`;
  }
}
```

The Lit migrated code would look as follows, with the observer code split
into `willUpdate()` and `updated()` based on whether it accesses the DOM:
```
static override get properties() {
  return {
   max: {type: Number},
   min: {type: Number},
   value: {type: Number},
 };
}

override willUpdate(changedProperties: PropertyValues<this>) {
  super.willUpdate(changedProperties);
  // Clamp value in willUpdate() so we don't trigger a second update
  // cycle for the same changes.
  if (changedProperties.has('min') || changedProperties.has('max') ||
      changedProperties.has('value')) {
    this.value = Math.min(Math.max(this.value, this.min), this.max);
  }
}

override updated(changedProperties: PropertyValues<this>) {
  super.updated(changedProperties);

  // Querying and modifying the DOM should happen in updated().
  if (changedProperties.has('value')) {
    const demo = this.shadowRoot!.querySelector('#demo');
    if (demo) {
      demo.style.height = `${this.value}px`;
    }
  }
}
```

### dom-if
Polymer `<template is="dom-if">` should generally be replaced by ternary
statements in the `.html.ts` file of the form
`${condition ? html`&lt;some-html>` : ''}`. Example simplified from
`cr-toolbar`:

Polymer `cr_toolbar.html`:
```
<div id="content">
  <template is="dom-if" if="[[showMenu]]" restamp>
    <cr-icon-button id="menuButton" class="no-overlap"
        iron-icon="cr20:menu" on-click="onMenuClick_">
    </cr-icon-button>
  </template>
  <h1>[[pageName]]</h1>
</div>
```

Lit `cr_toolbar.html.ts`:
```
<div id="content">
  ${this.showMenu ? html`
    <cr-icon-button id="menuButton" class="no-overlap"
        iron-icon="cr20:menu" @click="${this.onMenuClick_}">
    </cr-icon-button>` : ''}
  <h1>${this.pageName}</h1>
</div>
```

***note
Lit conditional rendering is specifically similar to a dom-if template
that uses `restamp` (like the example above). This represents the vast majority
of cases in Chromium WebUI code.
***

### dom-repeat
Polymer `<template is="dom-repeat">` should generally be replaced by a `map()`
call, of the form
`${this.myItems.map((item, index) => html`&lt;div>item.name</div>`)}`.

Unlike in Polymer where events triggered from elements in the template are
augmented with data about the item and index they are associated with (i.e. the
`DomRepeatEvent` data), event handlers connected to elements in a repeated
subtree in Lit receive the original event without any additional data.

*** promo
When migrating to Lit, event handlers that use the `DomRepeatEvent'`s `item`
and/or `index` need to use a different method to get this information.
***

One possibility is to set the index or item as data attributes on elements that
fire events, as seen in the example that follows.

From the Polymer element template:
```
<template is="dom-repeat" items="[[listItems]]">
  <div class="item-container [[getSelectedClass_(item, selectedItem)]]">
    <cr-button id="[[getItemId_(index)]]" on-click="onItemClick_">
      [[item.name]]
    </cr-button>
  </div>
</template>
```

From the Polymer element definition:
```
private getItemId_(index: number): string {
  return 'listItemId' + index;
}

private getSelectedClass_(item: ListItemType): string {
  return (item === this.selectedItem) ? 'selected' : '';
}

private onItemClick_(e: DomRepeatEvent<ListItemType>) {
  this.selectedItem = e.model.item;
  // Autoscroll to selected item if it is not completely visible.
  const list =
      this.shadowRoot!.querySelectorAll<HTMLElement>('.item-container');
  const selectedElement = list[e.model.index];
  assert(selectedElement!.classList.contains('selected'));
  selectedElement!.scrollIntoViewIfNeeded();
}
```

Lit template:
```
${this.listItems.map((item, index) => html`
  <div class="item-container ${this.getSelectedClass_(item)}">
    <cr-button id="${this.getItemId_(index)}"
        data-index="${index}" @click="${this.onItemClick_}">
      ${item.name}
    </cr-button>
  </div>
`)}
```
***note
Note the `data-index` setting the `data` attribute on the
`cr-button` that triggers the click handler.
***

From the Lit element definition file:
```
protected getItemId_(index: number): string {
  return 'listItemId' + index;
}

protected getSelectedClass_(item: ListItemType): string {
  return item === this.selectedItem ? 'selected' : '';
}

protected onItemClick_(e: Event) {
  const currentTarget = e.currentTarget as HTMLElement;

  // Use dataset to get the index set in the .html.ts template.
  const index = Number(currentTarget.dataset['index']);
  this.selectedItem = this.listItems[index];

  // Autoscroll to selected item if it is not completely visible.
  const list =
      this.shadowRoot!.querySelectorAll<HTMLElement>('.item-container');
  const selectedElement = list[index];
  selectedElement!.scrollIntoViewIfNeeded();
}
```

### Using composition for more complex dom-if/dom-repeat cases
In more complex cases, composition in the Lit `.html.ts` file may be more
readable and easier to maintain than directly replacing dom-ifs and dom-repeats
as described above. Composition involves the use of helper functions called from
the main `getHtml()` function to define portions of the element’s HTML template.

Cases where this has proven useful include:
1. Nested `<template is="dom-if">` and/or `<template is="dom-repeat">`
2. dom-repeats using the `filter` option

An example based on a simplified form of `cr-url-list-item`, which uses
composition, follows.

From the Polymer `.html` template:
```
<div class="folder-and-count">
  <template is="dom-if" if="[[shouldShowFolderImages_(size)]]" restamp>
    <template is="dom-repeat" items="[[imageUrls]]"
        filter="shouldShowImageUrl_">
      <div class="image-container" hidden$="[[!firstImageLoaded_]]">
        <img is="cr-auto-img" auto-src="[[item]]" draggable="false">
      </div>
    </template>
  </template>
  <div class="count">[[getDisplayedCount_(count)]]</div>
</div>
```

From the Polymer element definition:
```
private shouldShowImageUrl_(_url: string, index: number) {
  return index <= 1;
}

private shouldShowFolderImages_(): boolean {
  return this.size !== CrUrlListItemSize.COMPACT;
}

private getDisplayedCount_() {
  if (this.count && this.count > 999) {
    // The square to display the count only fits 3 characters.
    return '99+';
  }

  return this.count;
}
```

From the Lit `.html.ts` template file:
```
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrUrlListItemElement} from './cr_url_list_item.js';

function getImageHtml(this: CrUrlListItemElement,
                      item: string, index: number) {
  // Replaces dom-repeat's |filter| property by returning empty if the
  // filter function returns false for this item and index.
  if (!this.shouldShowImageUrl_(item, index)) {
    return '';
  }

  return html`
<div class="image-container" ?hidden="${!this.firstImageLoaded_}">
  <img is="cr-auto-img" auto-src="${item}" draggable="false">
</div>`;
}

function getFolderImagesHtml(this: CrUrlListItemElement) {
  // Replaces dom-if by returning empty string if condition is false.
  if (!this.shouldShowFolderImages_()) {
    return '';
  }

  // Replaces dom-repeat with map()
  return html`${
      this.imageUrls.map(
          (item, index) => getImageHtml.bind(this)(item, index))}`;
}

export function getHtml(this: CrUrlListItemElement) {
  return html`
/* other content here */
  <div class="folder-and-count">
    ${getFolderImagesHtml.bind(this)()}
    <div class="count">${this.getDisplayedCount_()}</div>
  </div>
/* other content */
`;
}
```

From the Lit element definition:
```
protected getDisplayedCount_(): string {
  if (this.count && this.count > 999) {
    // The square to display the count only fits 3 characters.
    return '99+';
  }

  return this.count === undefined ? '' : this.count.toString();
}

protected shouldShowImageUrl_(_url: string, index: number): boolean {
  return index <= 1;
}

protected shouldShowFolderImages_(): boolean {
  return this.size !== CrUrlListItemSize.COMPACT;
}
```

### Migrating iron-list clients
There are a few considerations when migrating `iron-list` clients.

First, many existing `iron-list` clients don't require virtualization
as the lists they render are bounded in size and not particularly large (e.g.
only ~100 items). Such clients should use Lit's `map()` directive.

If the `iron-list` client is actually rendering a very large number of items,
some lazy rendering may be necessary. `cr-infinite-list` replicates the
focus and navigation behavior of `iron-list`. It uses `cr-lazy-list`
internally to render items.

`cr-lazy-list` adds list items to the DOM lazily as the user scrolls to them.
It also leverages CSS `content-visibility` to avoid rendering work for items
not in the viewport. If custom navigation or focus behavior (i.e. different
from `iron-list`) is desired, `cr-lazy-list` can be used directly as it is in
the Tab Search Page's `selectable-lazy-list`.

If you do not think any of the 3 options above are suitable for a list you
are migrating or adding to a WebUI, reach out to the WebUI team.

For incremental migrations, it may be useful to migrate `iron-list` children
prior to migrating the `iron-list` client itself. This can be somewhat
complicated by `iron-list` manually positioning its items, meaning it must
always know when its children change size. When migrating `iron-list`
children, the child elements must manually fire an `iron-resize` event from
their `updated()` lifecycle callback whenever any property that may impact
their height has changed. See example below:

From the `list_parent.html` template (`iron-list` client so must be Polymer)
```
<iron-list id="list" items="[[listItems_]]" as="item">
  <template>
    <custom-item description="[[item.description]]" name="[[item.name]]"
        on-click="onListItemClick_">
    </custom-item>
  </template>
</iron-list>
```

From the child `custom_item.html.ts` template:
```
<div class="name">${this.name}</div>
<div class="description" ?hidden="${!this.description}">
  ${this.description}
</div>
```

In this case, the value of `description` impacts the height of the child
item. If `iron-list` is not notified of when the child is done with rendering
a change to this property, it may compute the child's height incorrectly, and
display gaps or overlap in the list. To prevent this, the child item should
fire `iron-resize` in `updated()` if its `description` property changes.

From `custom_item.ts`:
```
override updated(changedProperties: PropertyValues<this>) {
  super.updated(changedProperties);
  if (changedProperties.has('description')) {
    this.fire('iron-resize');
  }
}
```

## Additional Lit and Polymer differences
### Testing

A large number of unit tests do something like the following:
```
// Validate that the input is disabled when invalid is set.
myTestElement.invalid = true;
assertTrue(myTestElement.$.input.disabled);
```

This assumes that setting `invalid` synchronously updates the DOM of the test
element. If the test element is a Lit-based element, this is no longer the case,
and we need to wait for a render cycle to complete. There are a couple of ways
to do this:

1. Preferred: Use `await microtasksFinished()` test helper method from
   chrome://webui-test/test\_util.js. This method awaits a setTimeout of 0 which
   allows any render cycles to complete (useful if there may be multiple Lit
   elements that need to finish updating before assertions).
2. Directly `await myTestElement.updateComplete` (waits for the test element’s
   render cycle).

Updated example:
```
// Validate that the input is disabled when invalid is set.
myTestElement.invalid = true;
await microtasksFinished();
assertTrue(myTestElement.$.input.disabled);
```

*** note
Note: for many test cases, it is less fragile to directly wait on an event
or BrowserProxy call that should be triggered by an action in a test, instead
of either assuming everything is synchronous or waiting on framework-dependent
test helpers like `microtasksFinished()` or the Polymer
`waitAfterNextRender()/flushTasks()` that do not actually guarantee that
anything specific has happened.
***

### Use of the `hidden` attribute
As documented in the [styleguide](https://chromium.googlesource.com/chromium/src/+/HEAD/styleguide/web/web.md#Polymer),
in Polymer the `hidden` attribute was recommended over `<template is="dom-if">`
for cases of showing and hiding small amounts of HTML or a single element. In
Lit, since conditional rendering does not rely on adding a custom element like
dom-if, there is not the same potential performance downside to using
conditional rendering instead of the `hidden` attribute.

***promo
In Lit, conditional rendering can be used instead of the
`hidden` attribute in most cases, and should always be used anywhere
`<template is="dom-if">` would previously have been used in Polymer code.
***
