// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Add functionality related to getting search engine details.
 */

/**
 * Encodes |url| in "application/x-www-form-urlencoded" content type of <form>.
 * The standard is defined in:
 * https://www.w3.org/TR/html4/interact/forms.html#h-17.13.4.1
 * This solution comes from:
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/encodeURIComponent
 * @private
 */
function encodeFormData_(url) {
  return encodeURIComponent(url).replace('%20', '+');
};

/**
 * Returns if |element| can submit a form(i.e. <button> or <input
 * type="submit">).
 * @param {Element} element An element inside a <form>.
 * @return {boolean} If |element| can submit the form.
 * @private
 */
function isSubmitElement_(element) {
  return (element.tagName == 'BUTTON') ||
      (element.tagName == 'INPUT' && element.type == 'submit');
};

/**
 * Returns if |element| is checkable(i.e. <input type="radio"> or <input
 * type="checkbox">).
 * @param {Element} element An element inside a <form>.
 * @return {boolean} If |element| is checkable.
 * @private
 */
function isCheckableElement_(element) {
  return (
      element.tagName == 'INPUT' &&
      (element.type == 'radio' || element.type == 'checkbox'));
};

/**
 * Records the active submit element of <form> being submitted.
 * @type {Element}
 * @private
 */
let activeSubmitElement_ = null;

/**
 * Returns the submit element which triggers the submission of |form|. If there
 * is no submit element clicked before |form|'s submission, the first submit
 * element of |form| will be returned.
 * @param {Element} form The <form> on submission.
 * @return {Element|undefined} The element which submits |form| or the first
 * submit element in |form|. Returns undefined if not found.
 * @private
 */
function getActiveSubmitElement_(form) {
  if (activeSubmitElement_ && activeSubmitElement_.form === form) {
    return activeSubmitElement_;
  }
  for (let i = 0; i < form.elements.length; ++i) {
    if (isSubmitElement_(form.elements[i])) {
      return form.elements[i];
    }
  }
};

/**
 * A set of all the text categories of <input>'s type attribute.
 * This set is based on:
 *   https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/html/forms/text_field_input_type.h
 *   https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/html/forms/base_text_input_type.h
 * "password" is not in the map because it makes the <form> invalid.
 * @type {Set<string>}
 * @private
 */
const textInputTypes_ =
    new Set(['email', 'search', 'tel', 'text', 'url', 'number']);

/**
 * Returns false if |element| is <input type="radio|checkbox"> or <select> and
 * it's not in its default state, otherwise true. The default state is the state
 * of the form element on initial load of the page, and leties depending upon
 * the form element.
 * @param {Element} element Element in <form>.
 * @return {boolean} If the element is in its default state.
 * @private
 */
function isInDefaultState_(element) {
  if (isCheckableElement_(element)) {
    return element.checked == element.defaultChecked;
  } else if (element.tagName == 'SELECT') {
    for (let i = 0; i < element.options.length; ++i) {
      let option = element.options[i];
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
 *   4. Has exactly one <input> with "type" from |textInputTypes_|;
 *   5. Has no element that is not in default state;
 * Any element that doesn't have "name" attribute or has "disabled" attribute
 * will be ignored.
 *
 * @param {Element} form The form being submitted.
 * @return {Element|undefined} The only one text <input> in |form|, or undefined
 *   if |form| is not a valid searchable <form>.
 * @private
 */
function findSuitableSearchInputElement_(form) {
  let result = undefined;
  for (let i = 0; i < form.elements.length; ++i) {
    let element = form.elements[i];
    if (element.disabled || !element.name) {
      continue;
    }
    if (!isInDefaultState_(element) || element.tagName == 'TEXTAREA') {
      return;
    }
    if (element.tagName == 'INPUT') {
      if (element.type == 'file' || element.type == 'password') {
        return;
      }
      if (textInputTypes_.has(element.type)) {
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
 * Generates a searchable URL from |form| if it's a valid searchable <form>.
 * The code is based on the function with same name in:
 *   https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/exported/web_searchable_form_data.cc
 *
 * @param {Element} form The <form> element.
 * @return {string|undefined} The searchable URL, or undefined if |form| is not
 *   a valid searchable <form>.
 * @private
 * TODO(crbug.com/433824): Use <form>'s "accept-charset" attribute to encode the
 *   searchableURL.
 */
function generateSearchableUrl_(form) {
  // Only consider <form> that navigates in current frame, because currently
  // TemplateURLs are created by SearchEngineTabHelper, which cannot handle
  // navigation across WebState.
  if (form.target && form.target != '_self')
    return;

  // Only consider forms that GET data.
  if (form.method && form.method.toLowerCase() != 'get') {
    return;
  }

  let searchInput = findSuitableSearchInputElement_(form);
  if (!searchInput) {
    return;
  }

  let activeSubmitElement = getActiveSubmitElement_(form);

  // The "name=value" pairs appended to the end of the action URL.
  let queryArgs = [];
  for (let i = 0; i < form.elements.length; ++i) {
    let element = form.elements[i];
    if (element.disabled || !element.name) {
      continue;
    }
    if (isSubmitElement_(element)) {
      // Only append the active submit element's name-value pair.
      if (element === activeSubmitElement) {
        let value = element.value;
        // <input type="submit"> will have "Submit" as default "value" when
        // submitted with empty "value" and non-empty "name". This probably
        // comes from the default label text of <input type="submit">:
        //   https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/submit
        if (element.tagName == 'INPUT' && !value) {
          value = 'Submit';
        }
        queryArgs.push(
            encodeFormData_(element.name) + '=' + encodeFormData_(value));
      }
      continue;
    }
    if (element === searchInput) {
      queryArgs.push(encodeFormData_(element.name) + '={searchTerms}');
    } else {
      // Ignore unchecked checkable element.
      if (isCheckableElement_(element) && !element.checked) {
        continue;
      }
      queryArgs.push(
          encodeFormData_(element.name) + '=' + encodeFormData_(element.value));
    }
  }
  // If |form| uses "GET" method, appended query args in |form|.action should be
  // dropped. Use URL class to get rid of these query args.
  let url = new URL(form.action);
  return url.origin + url.pathname + '?' + queryArgs.join('&');
};

/**
 * Adds listener for 'click' event on |document|. When a submit element is
 * clicked, records it in |activeSubmitElement_| for |generateSearchableUrl_|,
 * which will be called in the 'submit' event callbacks within current call
 * stack. Appends a callback at the end of Js task queue with timeout=0ms that
 * sets |activeSubmitElement_| back to undefined after the submission.
 *
 * The call stack of form submission:
 *   User clicks button.
 *     "click" event emitted and bubbles up to |document|.
 *       Records current button as |activeSubmitElement_|.
 *       Posts callback that unsets active submit element by setTimeout(..., 0).
 *     "click" event ends.
 *     "submit" event emitted and bubbles up to |document|.
 *       Generates searchable URL based on |activeSubmitElement_|.
 *     "submit" event ends.
 *   Call stack of user's click on button finishes.
 *   ...
 *   Js task queue running...
 *   ...
 *   Callback posted by setTimeout(..., 0) is invoked and clean up
 *   |activeSubmitElement_|.
 */
document.addEventListener('click', function(event) {
  if (event.defaultPrevented) {
    return;
  }
  let element = event.target;
  if (!(element instanceof Element) || !isSubmitElement_(element)) {
    return;
  }
  activeSubmitElement_ = element;
  setTimeout(function() {
    if (activeSubmitElement_ === element) {
      activeSubmitElement_ = null;
    }
  }, 0);
});

/**
 * Adds listener for 'submit' event on |document|. When a <form> is submitted,
 * try to generate a searchableUrl. If succeeded, send it back to native code.
 * TODO(crbug.com/433824): Refactor /components/autofill/ios/form_util to reuse
 *   FormActivityObserver, so that all the data about form submission can be
 *   sent in a single message.
 */
document.addEventListener('submit', function(event) {
  if (event.defaultPrevented || !(event.target instanceof Element)) {
    return;
  }
  let url = generateSearchableUrl_(event.target);
  if (url) {
    __gCrWeb.common.sendWebKitMessage( 'SearchEngineMessage',
        {'command': 'searchableUrl', 'url': url});
  }
}, false);

/**
 * Finds <link> of OSDD(Open Search Description Document) in the main frame. If
 * found, sends a message containing the page's URL and OSDD's URL to native
 * side. If the page has multiple OSDD <links>s (which should never happen on a
 * sane web site), only send the first <link>.
 * @return {undefined}
 */
function findOpenSearchLink() {
  let links = document.getElementsByTagName('link');
  for (let i = 0; i < links.length; ++i) {
    if (links[i].type == 'application/opensearchdescription+xml') {
      __gCrWeb.common.sendWebKitMessage( 'SearchEngineMessage', {
        'command': 'openSearch',
        'pageUrl': document.URL,
        'osddUrl': links[i].href
      });
      return;
    }
  };
}

// If document is loaded, finds the Open Search <link>, otherwise waits until
// it's loaded and then starts finding.
if (document.readyState == 'complete') {
  findOpenSearchLink();
} else {
  window.addEventListener('load', findOpenSearchLink);
}
