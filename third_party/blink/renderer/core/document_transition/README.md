# Document Transitions

This directory contains the script interface and implementation of the Document
Transition, and Shared Element Transition APIs.

Document Transition is a type of an animated transition that allows content to
animate to a new DOM state easily. For instance, modifying the DOM to change the
background color is a change that can easily be done without document
transitions. However, document transition also allows the new background state
to, for example, slide in from the left instead of simply atomically appearing
on top of the content.

For a detailed explanation, please see the
[explainer](https://github.com/vmpstr/shared-element-transitions/blob/main/README.md)

## Code Structure

A new method is exposed on window.document, called createTransition(). This is
the main interface to getting a new transition object from JavaScript. It is
specified in the `document_create_transition.idl` and is implemented in
corresponding `.cc` and `.h` files.

When called, `createTransition()` constructs a DocumentTransition object which
is specified in `document_transition.idl` and is implemented in corresponding
`.cc` and `.h` files.

The rest of the script interactions happen with this object.

## Additional Notes

Note that this project is in early stages of design and implementation. To
follow the design evolution, please see [our github
repo](https://github.com/vmpstr/shared-element-transitions/). Furthermore, this
README's Code Structure section will be updated as we make progress with our
implementation.
