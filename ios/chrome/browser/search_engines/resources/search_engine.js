// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Add functionality related to getting image data.
 */
goog.provide('__crWeb.searchEngine');

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.searchEngine = {};

/**
 * Store common namespace object in a global __gCrWeb object referenced by a
 * string, so it does not get renamed by closure compiler during the
 * minification.
 */
__gCrWeb['searchEngine'] = __gCrWeb.searchEngine;

/* Beginning of anonymous object. */
(function() {

/**
 * Find <link> of OSDD(Open Search Description Document) in document and return
 * it's URL. If multiple OSDDs are found(which should never happen on a sane web
 * site), return the URL of the first OSDD.
 * @return {string|undefined} "href" of OSDD <link>, or undefined if not found.
 */
__gCrWeb.searchEngine.getOpenSearchDescriptionDocumentUrl = function() {
  var links = document.getElementsByTagName('link');
  for (var i = 0; i < links.length; ++i) {
    if (links[i].type == 'application/opensearchdescription+xml') {
      return links[i].href;
    }
  }
};

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
var activeSubmitElement_ = null;

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
  for (var i = 0; i < form.elements.length; ++i) {
    if (isSubmitElement_(form.elements[i])) {
      return form.elements[i];
    }
  }
};

/**
 * A set of all the text categories of <input>'s type attribute.
 * This set is based on:
 * https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/html/forms/base_text_input_type.h?rcl=3e6185be26ac5e48c7921d314d2abc4a3a1572e2&l=40
 * https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/html/forms/text_field_input_type.h?rcl=3e6185be26ac5e48c7921d314d2abc4a3a1572e2&l=41
 * "password" is not in the map because it makes the <form> invalid.
 * @type {Set<string>}
 * @private
 */
var textInputTypes_ =
    new Set(['email', 'search', 'tel', 'text', 'url', 'number']);

/**
 * Returns false if |element| is <input type="radio|checkbox"> or <select> and
 * it's not in its default state, otherwise true. The default state is the state
 * of the form element on initial load of the page, and varies depending upon
 * the form element. The code is based on:
 * https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/exported/web_searchable_form_data.cc?rcl=a26f1d82bbd020be06fb96518179bbb6b6146370&l=120
 * @param {Element} element Element in <form>.
 * @return {boolean} If the element is in its default state.
 * @private
 */
function isInDefaultState_(element) {
  if (isCheckableElement_(element)) {
    return element.checked == element.defaultChecked;
  } else if (element.tagName == 'SELECT') {
    for (var i = 0; i < element.options.length; ++i) {
      var option = element.options[i];
      if (option.selected != option.defaultSelected) {
        return false;
      }
    }
  }
  return true;
};

/**
 * Looks for a suitable search text field in |form|. Returns undefined if |form|
 * is not a valid searchable <form>. The code is based on:
 * https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/exported/web_searchable_form_data.cc?rcl=a26f1d82bbd020be06fb96518179bbb6b6146370&l=137
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
  var result = undefined;
  for (var i = 0; i < form.elements.length; ++i) {
    var element = form.elements[i];
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
 * The code is based on:
https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/exported/web_searchable_form_data.cc?rcl=9c80632d2b16884970961dc9a12bddd3a4ca50b5&l=218
 *
 * @param {Element} form The <form> element.
 * @return {string|undefined} The searchable URL, or undefined if |form| is not
 *   a valid searchable <form>.
 * @private
 * TODO(crbug.com/433824): Use <form>'s "accept-charset" attribute to encode the
 *   searchableURL.
 */
function generateSearchableUrl_(form) {
  // Only consider forms that GET data.
  if (form.method && form.method.toLowerCase() != 'get') {
    return;
  }

  var searchInput = findSuitableSearchInputElement_(form);
  if (!searchInput) {
    return;
  }

  var activeSubmitElement = getActiveSubmitElement_(form);

  // The "name=value" pairs appended to the end of the action URL.
  var queryArgs = [];
  for (var i = 0; i < form.elements.length; ++i) {
    var element = form.elements[i];
    if (element.disabled || !element.name) {
      continue;
    }
    if (isSubmitElement_(element)) {
      // Only append the active submit element's name-value pair.
      if (element === activeSubmitElement) {
        var value = element.value;
        // <input type="submit"> will have "Submit" as default "value" when
        // submitted with empty "value" and non-empty "name". This probably
        // comes from the default label text of <input type="submit">:
        //   https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/submit
        if (element.tagName == 'INPUT' && !value) {
          value = 'Submit';
        }
        queryArgs.push(
            encodeFormData_(element.name) + '=' +
            encodeFormData_(value));
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
  var url = new URL(form.action);
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
  var element = event.target;
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
  var url = generateSearchableUrl_(event.target);
  if (url) {
    __gCrWeb.message.invokeOnHost(
        {'command': 'searchEngine.searchableUrl', 'url': url});
  }
}, false);

}());  // End of anonymous object
