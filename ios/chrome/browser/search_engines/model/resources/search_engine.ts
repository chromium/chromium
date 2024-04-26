// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Add functionality related to getting search engine details.
 */

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Encodes `url` in "application/x-www-form-urlencoded" content type of <form>.
 * The standard is defined in:
 * https://www.w3.org/TR/html4/interact/forms.html#h-17.13.4.1
 * This solution comes from:
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/encodeURIComponent
 */
function encodeFormData(url: string): string {
  return encodeURIComponent(url).replace('%20', '+');
};

/**
 * Returns element if it's of a type that can submit a form or null otherwise.
 */
function asSubmitElement(element: Element): HTMLButtonElement|HTMLInputElement|
    null {
  if (element instanceof HTMLButtonElement) {
    return element;
  }
  if (element instanceof HTMLInputElement && element.type === 'submit') {
    return element;
  }
  return null;
};

/**
 * Returns the value stored in the element's `name` property. If the
 * element does not have the name property, then null is returned.
 */
function getElementName(element: Element): string|null {
  return (element as Element & {'name': string}).name;
}

/**
 * Returns whether the element is disabled. If the element does not have the
 * disabled property, then null is returned.
 */
function isDisabledElement(element: Element): boolean {
  return (element as Element & {'disabled': boolean}).disabled;
}

/**
 * Returns if `element` is checkable(i.e. <input type="radio"> or <input
 * type="checkbox">).
 * @param element An element inside a <form>.
 */
function isCheckableElement(element: Element): boolean {
  return element instanceof HTMLInputElement &&
      (element.type === 'radio' || element.type === 'checkbox');
};

// Records the active submit element of <form> being submitted.
let activeSubmitElement: HTMLButtonElement|HTMLInputElement|null = null;

/**
 * Returns the submit element which triggers the submission of `form`. If there
 * is no submit element clicked before `form`'s submission, the first submit
 * element of `form` will be returned. Otherwise, returns undefined if not
 * found.
 * @param form The <form> on submission.
 */
function getActiveSubmitElement(form: HTMLFormElement): HTMLButtonElement|
    HTMLInputElement|null {
  if (activeSubmitElement && activeSubmitElement.form === form) {
    return activeSubmitElement;
  }
  for (const element of form.elements) {
    const submitElement = asSubmitElement(element);
    if (submitElement) {
      return submitElement
    }
  }

  return null;
};

/**
 * A set of all the text categories of <input>'s type attribute.
 * This set is based on:
 *   https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/html/forms/text_field_input_type.h
 *   https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/html/forms/base_text_input_type.h
 * "password" is not in the map because it makes the <form> invalid.
 */
const TEXT_INPUT_TYPES =
    new Set(['email', 'search', 'tel', 'text', 'url', 'number']);

/**
 * Returns false if `element` is <input type="radio|checkbox"> or <select> and
 * it's not in its default state, otherwise true. The default state is the state
 * of the form element on initial load of the page, and leties depending upon
 * the form element.
 * @param element an Element in <form>.
 */
function isInDefaultState(element: Element): boolean {
  if (isCheckableElement(element)) {
    const inputElement = element as HTMLInputElement;
    return inputElement.checked === inputElement.defaultChecked;
  }

  if (element instanceof HTMLSelectElement) {
    for (const option of element.options) {
      if (option.selected != option.defaultSelected) {
        return false;
      }
    }
  }

  return true;
};

/**
 * Looks for a suitable search text field in |form|. Returns undefined if |form|
 * is not a valid searchable <form>. The code is based on the function with same
 * name in:
 *   https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/exported/web_searchable_form_data.cc
 *
 * The criteria for a valid searchable <form>:
 *   1. Has no <textarea>;
 *   2. Has no <input type="password">;
 *   3. Has no <input type="file">;
 *   4. Has exactly one <input> with "type" from `TEXT_INPUT_TYPES_`;
 *   5. Has no element that is not in default state;
 * Any element that doesn't have "name" attribute or has "disabled" attribute
 * will be ignored.
 * @param form The form being submitted.
 */
function findSuitableSearchInputElement(form: HTMLFormElement):
    HTMLInputElement|undefined {
  let result: HTMLInputElement|undefined = undefined;
  for (const element of form.elements) {
    if (isDisabledElement(element) || !getElementName(element)) {
      continue;
    }
    if (!isInDefaultState(element) || element instanceof HTMLTextAreaElement) {
      return;
    }
    if (element instanceof HTMLInputElement) {
      if (element.type === 'file' || element.type === 'password') {
        return;
      }
      if (TEXT_INPUT_TYPES.has(element.type)) {
        if (result) {
          return;
        }
        result = element;
      }
    }
  }
  return result;
};

/**
 * Generates a searchable URL from `form` if it's a valid searchable <form>.
 * The code is based on the function with same name in:
 *   https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/exported/web_searchable_form_data.cc
 * TODO(crbug.com/40394195): Use <form>'s "accept-charset" attribute to encode the
 *   searchableURL.
 */
function generateSearchableUrl(form: Element): string|undefined {
  if (!(form instanceof HTMLFormElement)) {
    return;
  }

  // Only consider <form> that navigates in current frame, because currently
  // TemplateURLs are created by SearchEngineTabHelper, which cannot handle
  // navigation across WebState.
  if (form.target && form.target !== '_self')
    return;

  // Only consider forms that GET data.
  if (form.method && form.method.toLowerCase() !== 'get') {
    return;
  }

  const searchInput = findSuitableSearchInputElement(form);
  if (!searchInput) {
    return;
  }

  const activeSubmitElement = getActiveSubmitElement(form);

  // The "name=value" pairs appended to the end of the action URL.
  const queryArgs: string[] = [];
  for (const element of form.elements) {
    const elementName = getElementName(element);
    if (isDisabledElement(element) || !elementName) {
      continue;
    }

    const submitElement = asSubmitElement(element);
    if (submitElement) {
      // Only append the active submit element's name-value pair.
      if (submitElement === activeSubmitElement) {
        let value = submitElement.value;
        // <input type="submit"> will have "Submit" as default "value" when
        // submitted with empty "value" and non-empty "name". This probably
        // comes from the default label text of <input type="submit">:
        //   https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/submit
        if (submitElement instanceof HTMLInputElement && !value) {
          value = 'Submit';
        }

        queryArgs.push(
            encodeFormData(elementName) + '=' + encodeFormData(value));
      }
      continue;
    }
    if (element === searchInput) {
      queryArgs.push(encodeFormData(elementName) + '={searchTerms}');
    } else {
      // Ignore unchecked checkable element.
      if (isCheckableElement(element) &&
          !(element as HTMLInputElement).checked) {
        continue;
      }
      const elementValue = (element as Element & {'value': string}).value;
      queryArgs.push(
          encodeFormData(elementName) + '=' + encodeFormData(elementValue));
    }
  }
  // If `form` uses "GET" method, appended query args in `form`.action should be
  // dropped. Use URL class to get rid of these query args.
  const url = new URL(form.action);
  return url.origin + url.pathname + '?' + queryArgs.join('&');
};

/**
 * Adds listener for 'click' event on `document`. When a submit element is
 * clicked, records it in `activeSubmitElement` for `generateSearchableUrl`,
 * which will be called in the 'submit' event callbacks within current call
 * stack. Appends a callback at the end of Js task queue with timeout=0ms that
 * sets `activeSubmitElement` back to undefined after the submission.
 *
 * The call stack of form submission:
 *   User clicks button.
 *     "click" event emitted and bubbles up to `document`.
 *       Records current button as `activeSubmitElement`.
 *       Posts callback that unsets active submit element by setTimeout(..., 0).
 *     "click" event ends.
 *     "submit" event emitted and bubbles up to `document`.
 *       Generates searchable URL based on `activeSubmitElement`.
 *     "submit" event ends.
 *   Call stack of user's click on button finishes.
 *   ...
 *   Js task queue running...
 *   ...
 *   Callback posted by setTimeout(..., 0) is invoked and clean up
 *   `activeSubmitElement`.
 */
document.addEventListener('click', function(event) {
  if (event.defaultPrevented) {
    return;
  }
  let element = event.target;

  if (!(element instanceof Element)) {
    return;
  }
  const submitElement = asSubmitElement(element);
  if (!submitElement) {
    return;
  }
  activeSubmitElement = submitElement;
  setTimeout(function() {
    if (activeSubmitElement === element) {
      activeSubmitElement = null;
    }
  }, 0);
});

/**
 * Adds listener for 'submit' event on `document`. When a <form> is submitted,
 * try to generate a searchableUrl. If succeeded, send it back to native code.
 * TODO(crbug.com/40394195): Refactor /components/autofill/ios/form_util to reuse
 *   FormActivityObserver, so that all the data about form submission can be
 *   sent in a single message.
 */
document.addEventListener('submit', function(event) {
  if (event.defaultPrevented || !(event.target instanceof Element)) {
    return;
  }
  const url = generateSearchableUrl(event.target);
  if (url) {
    sendWebKitMessage(
        'SearchEngineMessage', {'command': 'searchableUrl', 'url': url});
  }
}, false);

/**
 * Finds <link> of OSDD(Open Search Description Document) in the main frame. If
 * found, sends a message containing the page's URL and OSDD's URL to native
 * side. If the page has multiple OSDD <links>s (which should never happen on a
 * sane web site), only send the first <link>.
 */
function findOpenSearchLink(): void {
  const links = document.getElementsByTagName('link');
  for (const link of links) {
    if (link.type == 'application/opensearchdescription+xml') {
      sendWebKitMessage('SearchEngineMessage', {
        'command': 'openSearch',
        'pageUrl': document.URL,
        'osddUrl': link.href
      });
      return;
    }
  };
}

// If document is loaded, finds the Open Search <link>, otherwise waits until
// it's loaded and then starts finding.
if (document.readyState === 'complete') {
  findOpenSearchLink();
} else {
  window.addEventListener('load', findOpenSearchLink);
}
