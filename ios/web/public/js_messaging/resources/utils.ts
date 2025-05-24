// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions used throughout iOS JavaScript.
 * It is important to note that import statements are merged into a single
 * script during compile time as WebKit doesn't support JS imports across
 * user scripts. This will have the effect of functions here being duplicated.
 * If it is important for your function to keep state, storing that state
 * globally or within a file which is injected only once is necessary. Global
 * state can be saved within __gCrWeb and accessed through functions here.
 * For more complex logic or to avoid global state, bundle a script on its own
 * via the gn `optimize_js` template and include the script as a user script.
 * Such a script can maintain state, for  example see `find_in_page.js`.
 * If access to the state is required outside of the single script file, API
 * can be exposed through `__gCrWeb` in the same way that functions are exposed
 * for calling from the native Objective-C++ code.
 */

declare global {
  interface Window {
    webkit: any;
  }
}

/**
 * Extracts the webpage URL from the given URL by removing the query
 * and the reference (aka fragment) from the URL.
 *
 * IMPORTANT: Not security proof, do not assume the URL returns by this
 * function reflects what is actually on the page as the hosted page can
 * modify the behavior of the window.URL prototype.
 *
 * @param url Web page URL.
 * @return Web page URL with query and reference removed. An empty
 *   string if the window.URL prototype was changed by the hosted page.
 */
export function removeQueryAndReferenceFromURL(url: string): string {
  if (typeof url !== 'string') {
    return '';
  }

  let parsed: URL | undefined;
  // Strings which are not URLs will throw a TypeError.
  try {
    parsed = new URL(url);
  } catch (error) {
    return '';
  }

  function isPropertyInvalid(value: unknown): boolean {
    return typeof value !== 'string';
  }

  if (isPropertyInvalid(parsed.origin) || isPropertyInvalid(parsed.protocol) ||
      isPropertyInvalid(parsed.pathname)) {
    // If at least one of these properties is not of a string type, it is a sign
    // that the window.URL prototype was changed by the hosted page in the page
    // content world. Return an empty string in that case as URL has an
    // undefined behavior. This doesn't cover all window.URL mutations, but it
    // at least shields against getting non-string values from these
    // properties. The returned URL will be malformed in the worst case but is
    // guaranteed to be a string.
    return '';
  }

  // For some protocols (eg. data:, javascript:) URL.origin is "null" string
  // (not the type) so URL.protocol is used instead.
  return (parsed.origin !== 'null' ? parsed.origin : parsed.protocol) +
      parsed.pathname;
}

/**
 * Posts `message` to the webkit message handler specified by `handlerName`.
 *
 * @param handlerName The name of the webkit message handler.
 * @param message The message to post to the handler.
 */
export function sendWebKitMessage(handlerName: string, message: object|string) {
  try {
    // A web page can override `window.webkit` with any value. Deleting the
    // object ensures that original and working implementation of
    // window.webkit is restored.
    const oldWebkit = window.webkit;
    delete window['webkit'];
    window.webkit.messageHandlers[handlerName].postMessage(message);
    window.webkit = oldWebkit;
  } catch (err) {
    // TODO(crbug.com/40269960): Report this fatal error
  }
}

/**
 * Trims any whitespace from the start and end of a string.
 * Used in preference to String.prototype.trim which can be overridden by
 * sites.
 *
 * @param str The string to be trimmed.
 * @return The string after trimming.
 */
export function trim(str: string): string {
  if (!str) {
    return '';
  }
  return str.replace(/^\s+|\s+$/g, '');
}

/**
 * Returns if an element is a text field.
 * This returns true for all of textfield-looking types such as text,
 * password, search, email, url, and number.
 *
 * This method aims to provide the same logic as method
 *     bool WebInputElement::isTextField() const
 * in chromium/src/third_party/WebKit/Source/WebKit/chromium/src/
 * WebInputElement.cpp, where this information is from
 *     bool HTMLInputElement::isTextField() const
 *     {
 *       return m_inputType->isTextField();
 *     }
 * (chromium/src/third_party/WebKit/Source/WebCore/html/HTMLInputElement.cpp)
 *
 * The implementation here is based on the following:
 *
 * - Method bool InputType::isTextField() defaults to be false and it is
 *   override to return true only in HTMLInputElement's subclass
 *   TextFieldInputType (chromium/src/third_party/WebKit/Source/WebCore/html/
 *   TextFieldInputType.h).
 *
 * - The implementation here considers all the subclasses of
 *   TextFieldInputType: NumberInputType and BaseTextInputType, which has
 *   subclasses EmailInputType, PasswordInputType, SearchInputType,
 *   TelephoneInputType, TextInputType, URLInputType. (All these classes are
 *   defined in chromium/src/third_party/WebKit/Source/WebCore/html/)
 *
 */
export function isTextField(element: Element): boolean {
  if (!(element instanceof HTMLInputElement) || element.type === 'hidden') {
    return false;
  }
  return [
    'text',
    'email',
    'password',
    'search',
    'tel',
    'url',
    'number',
  ].includes(element.type);
}

/**
 * Generates a 128-bit cryptographically-strong random number. The properties
 * must match base::UnguessableToken, as these values may be deserialized into
 * that class on the C++ side.
 * @return the generated number as a hex string.
 */
export function generateRandomId(): string {
  // Generate 128 bit unique identifier.
  const components = new Uint32Array(4);
  window.crypto.getRandomValues(components);
  return components.reduce(
      (id = '', component) => id + component.toString(16).padStart(8, '0'), '');
}
