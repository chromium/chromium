
See crbug.com/1446498 for more context.

The tests in this sub-folder only test Mutation Events. They were relocated here
from their original location (keeping the relative path structure) so that when
Mutation Events are removed, this entire folder can simply be deleted.

These tests remain *outside* this folder, but still use Mutation Events,
because it isn't clear whether they should be re-written using other tech:

- accessibility/details-summary-crash.html
- editing/pasteboard/drag-and-drop-image-contenteditable.html
- editing/pasteboard/drag-and-drop-inputimage-contenteditable.html
- editing/pasteboard/drag-and-drop-objectimage-contenteditable.html
- fast/dom/HTMLElement/set-inner-outer-optimization.html
- fast/dom/MutationObserver/observe-attributes.html
- fast/dom/MutationObserver/observe-characterdata.html
- fast/dom/dom-method-document-change.html
- fast/dom/move-nodes-across-documents.html
- fast/dom/shadow/shadow-boundary-crossing.html
- fast/events/window-onerror-isolatedworld-02.html
- http/tests/xmlhttprequest/reentrant-cancel-abort.html
- http/tests/xmlhttprequest/reentrant-cancel.html
- inspector-protocol/sessions/log-entry-added.js


