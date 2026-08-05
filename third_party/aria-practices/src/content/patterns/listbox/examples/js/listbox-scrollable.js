/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 */

'use strict';

/**
 * @namespace aria
 * @description
 * The aria namespace is used to support sharing class definitions between example files
 * without causing eslint errors for undefined classes
 */
var aria = aria || {};

/**
 * ARIA Scrollable Listbox Example
 *
 * @function onload
 * @description Initialize the listbox example once the page has loaded
 */

window.addEventListener('load', function () {
  new aria.Listbox(document.getElementById('ss_elem_list'));
});
