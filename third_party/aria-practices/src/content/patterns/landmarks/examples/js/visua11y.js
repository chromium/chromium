/*
 * Copyright 2011-2014 University of Illinois
 * Authors: Nicholas Hoyt
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 "use strict";

var _typeof = typeof Symbol === "function" && typeof Symbol.iterator === "symbol" ? function (obj) { return typeof obj; } : function (obj) { return obj && typeof Symbol === "function" && obj.constructor === Symbol ? "symbol" : typeof obj; };

/*
*   headings.js: highlight heading elements
*/

function initHeadings() {

  addPolyfills();

  var targetList = [{ selector: "h1", color: "navy",   label: "h1" },
                    { selector: "h2", color: "olive",  label: "h2" },
                    { selector: "h3", color: "purple", label: "h3" },
                    { selector: "h4", color: "green",  label: "h4" },
                    { selector: "h5", color: "gray",   label: "h5" },
                    { selector: "h6", color: "brown",  label: "h6" }];

  var selectors = targetList.map(function (tgt) {
    return tgt.selector;
  }).join(', ');

  function getInfo(element, target) {
    var info = new InfoObject(element, 'HEADING INFO');
    info.addProps('level ' + target.label.substring(1));
    return info;
  }

  var params = {
    appName: "Headings",
    cssClass: getCssClass("Headings"),
    msgText: "No heading elements (" + selectors + ") found.",
    targetList: targetList,
    getInfo: getInfo,
    dndFlag: true
  };

  return new Bookmarklet(params);
}

/*
*   landmarks.js: highlight ARIA landmarks
*/

function initLandmarks() {

  addPolyfills();

  // Filter function called on a list of elements returned by selector
  // 'footer, [role="contentinfo"]'. It returns true for the following
  // conditions: (1) element IS NOT a footer element; (2) element IS a
  // footer element AND IS NOT a descendant of article or section.
  function isContentinfo(element) {
    if (element.tagName.toLowerCase() !== 'footer') return true;
    if (!isDescendantOf(element, ['article', 'section'])) return true;
    return false;
  }

  // Filter function called on a list of elements returned by selector
  // 'header, [role="banner"]'. It returns true for the following
  // conditions: (1) element IS NOT a header element; (2) element IS a
  // header element AND IS NOT a descendant of article or section.
  function isBanner(element) {
    if (element.tagName.toLowerCase() !== 'header') return true;
    if (!isDescendantOf(element, ['article', 'section'])) return true;
    return false;
  }

  var targetList = [{ selector: 'aside:not([role]), [role~="complementary"], [role~="COMPLEMENTARY"]', color: "maroon", label: "complementary" },
                    { selector: 'form[aria-labelledby], form[aria-label], form[title], [role~="form"], [role~="form"]', color: "maroon", label: "form" },
                    { selector: 'footer, [role~="contentinfo"], [role~="CONTENTINFO"]', filter: isContentinfo, color: "olive", label: "contentinfo" },
                    { selector: '[role~="application"], [role~="APPLICATION"]', color: "black", label: "application" },
                    { selector: 'nav, [role~="navigation"], [role~="NAVIGATION"]', color: "green", label: "navigation" },
                    { selector: '[role~="region"][aria-labelledby], [role~="REGION"][aria-labelledby]', color: "teal", label: "region" },
                    { selector: '[role~="region"][aria-label], [role~="REGION"][aria-label]', color: "teal", label: "region" },
                    { selector: 'section[aria-labelledby], section[aria-label]', color: "teal", label: "region" },
                    { selector: 'header, [role~="banner"], [role~="BANNER"]', filter: isBanner, color: "gray", label: "banner" },
                    { selector: '[role~="search"], [role~="SEARCH"]', color: "purple", label: "search" },
                    { selector: 'main, [role~="main"], [role~="MAIN"]', color: "navy", label: "main" }];

  var selectors = targetList.map(function (tgt) {
    return '<li>' + tgt.selector + '</li>';
  }).join('');

  function getInfo(element, target) {
    return new InfoObject(element, 'LANDMARK INFO');
  }

  var params = {
    appName: "Landmarks",
    cssClass: getCssClass("Landmarks"),
    msgText: "No elements with ARIA Landmark roles found: <ul>" + selectors + "</ul>",
    targetList: targetList,
    getInfo: getInfo,
    dndFlag: true
  };

  return new Bookmarklet(params);
}

/*
*   Bookmarklet.js
*/

/* eslint no-console: 0 */
function logVersionInfo(appName) {
  console.log(getTitle() + ' : v' + getVersion() + ' : ' + appName);
}

function Bookmarklet(params) {
  var globalName = getGlobalName(params.appName);

  // use singleton pattern
  if (_typeof(window[globalName]) === 'object') return window[globalName];

  this.appName = params.appName;
  this.cssClass = params.cssClass;
  this.msgText = params.msgText;
  this.params = params;
  this.show = false;

  var dialog = new MessageDialog();
  window.addEventListener('resize', function () {
    removeNodes(this.cssClass);
    dialog.resize();
    this.show = false;
  });

  window[globalName] = this;
  logVersionInfo(this.appName);
}

Bookmarklet.prototype.run = function () {
  var dialog = new MessageDialog();

  dialog.hide();
  this.show = !this.show;

  if (this.show) {
    if (addNodes(this.params) === 0) {
      dialog.show(this.appName, this.msgText);
      this.show = false;
    }
  } else {
    removeNodes(this.cssClass);
  }

  return this.show;
};

/*
*   InfoObject.js
*/

/*
*  nameIncludesDescription: Determine whether accName object's name
*  property includes the accDesc object's name property content.
*/
function nameIncludesDescription(accName, accDesc) {
  if (accName === null || accDesc === null) return false;

  var name = accName.name,
      desc = accDesc.name;
  if (typeof name === 'string' && typeof desc === 'string') {
    return name.toLowerCase().includes(desc.toLowerCase());
  }

  return false;
}

function InfoObject(element, title) {
  this.title = title;
  this.element = getElementInfo(element);
  this.grpLabels = getGroupingLabels(element);
  this.accName = getAccessibleName(element);
  this.accDesc = getAccessibleDesc(element);
  this.role = getAriaRole(element);

  // Ensure that accessible description is not a duplication
  // of accessible name content. If it is, nullify the desc.
  if (nameIncludesDescription(this.accName, this.accDesc)) {
    this.accDesc = null;
  }
}

InfoObject.prototype.addProps = function (val) {
  this.props = val;
};

/*
*   constants.js
*/

var CONSTANTS = {};
Object.defineProperty(CONSTANTS, 'classPrefix', { value: 'a11yGfdXALm' });
Object.defineProperty(CONSTANTS, 'globalPrefix', { value: 'a11y' });
Object.defineProperty(CONSTANTS, 'title', { value: 'oaa-tools/bookmarklets' });
Object.defineProperty(CONSTANTS, 'version', { value: '0.2.2' });

function getCssClass(appName) {
  var prefix = CONSTANTS.classPrefix;

  switch (appName) {
    case 'Forms':
      return prefix + '0';
    case 'Headings':
      return prefix + '1';
    case 'Images':
      return prefix + '2';
    case 'Landmarks':
      return prefix + '3';
    case 'Lists':
      return prefix + '4';
    case 'Interactive':
      return prefix + '5';
  }

  return 'unrecognizedName';
}

function getGlobalName(appName) {
  return CONSTANTS.globalPrefix + appName;
}

function getTitle() {
  return CONSTANTS.title;
}
function getVersion() {
  return CONSTANTS.version;
}

/*
*   dialog.js: functions for creating, modifying and deleting message dialog
*/

/*
*   setBoxGeometry: Set the width and position of message dialog based on
*   the width of the browser window. Called by functions resizeMessage and
*   createMsgOverlay.
*/
function setBoxGeometry(dialog) {
  var width = window.innerWidth / 3.2;
  var left = window.innerWidth / 2 - width / 2;
  var scroll = getScrollOffsets();

  dialog.style.width = width + "px";
  dialog.style.left = scroll.x + left + "px";
  dialog.style.top = scroll.y + 30 + "px";
}

/*
*   createMsgDialog: Construct and position the message dialog whose
*   purpose is to alert the user when no target elements are found by
*   a bookmarklet.
*/
function createMsgDialog(cssClass, handler) {
  var dialog = document.createElement("div");
  var button = document.createElement("button");

  dialog.className = cssClass;
  setBoxGeometry(dialog);

  button.onclick = handler;

  dialog.appendChild(button);
  document.body.appendChild(dialog);
  return dialog;
}

/*
*   deleteMsgDialog: Use reference to delete message dialog.
*/
function deleteMsgDialog(dialog) {
  if (dialog) document.body.removeChild(dialog);
}

/*
*   MessageDialog: Wrapper for show, hide and resize methods
*/
function MessageDialog() {
  this.GLOBAL_NAME = 'a11yMessageDialog';
  this.CSS_CLASS = 'oaa-message-dialog';
}

/*
*   show: show message dialog
*/
MessageDialog.prototype.show = function (title, message) {
  var MSG_DIALOG = this.GLOBAL_NAME;
  var h2, div;

  if (!window[MSG_DIALOG]) window[MSG_DIALOG] = createMsgDialog(this.CSS_CLASS, function () {
    this.hide();
  });

  h2 = document.createElement("h2");
  h2.innerHTML = title;
  window[MSG_DIALOG].appendChild(h2);

  div = document.createElement("div");
  div.innerHTML = message;
  window[MSG_DIALOG].appendChild(div);
};

/*
*   hide: hide message dialog
*/
MessageDialog.prototype.hide = function () {
  var MSG_DIALOG = this.GLOBAL_NAME;
  if (window[MSG_DIALOG]) {
    deleteMsgDialog(window[MSG_DIALOG]);
    delete window[MSG_DIALOG];
  }
};

/*
*   resize: resize message dialog
*/
MessageDialog.prototype.resize = function () {
  var MSG_DIALOG = this.GLOBAL_NAME;
  if (window[MSG_DIALOG]) setBoxGeometry(window[MSG_DIALOG]);
};

/*
*   dom.js: functions and constants for adding and removing DOM overlay elements
*/

/*
*   isVisible: Recursively check element properties from getComputedStyle
*   until document element is reached, to determine whether element or any
*   of its ancestors has properties set that affect its visibility. Called
*   by addNodes function.
*/
function isVisible(element) {

  function isVisibleRec(el) {
    if (el.nodeType === Node.DOCUMENT_NODE) return true;

    var computedStyle = window.getComputedStyle(el, null);
    var display = computedStyle.getPropertyValue('display');
    var visibility = computedStyle.getPropertyValue('visibility');
    var hidden = el.getAttribute('hidden');
    var ariaHidden = el.getAttribute('aria-hidden');

    if (display === 'none' || visibility === 'hidden' || hidden !== null || ariaHidden === 'true') {
      return false;
    }
    return isVisibleRec(el.parentNode);
  }

  return isVisibleRec(element);
}

/*
*   countChildrenWithTagNames: For the specified DOM element, return the
*   number of its child elements with tagName equal to one of the values
*   in the tagNames array.
*/
function countChildrenWithTagNames(element, tagNames) {
  var count = 0;

  var child = element.firstElementChild;
  while (child) {
    if (tagNames.indexOf(child.tagName) > -1) count += 1;
    child = child.nextElementSibling;
  }

  return count;
}

/*
*   isDescendantOf: Determine whether element is a descendant of any
*   element in the DOM with a tagName in the list of tagNames.
*/
function isDescendantOf(element, tagNames) {
  if (typeof element.closest === 'function') {
    return tagNames.some(function (name) {
      return element.closest(name) !== null;
    });
  }
  return false;
}

/*
*   hasParentWithName: Determine whether element has a parent with
*   tagName in the list of tagNames.
*/
function hasParentWithName(element, tagNames) {
  var parentTagName = element.parentElement.tagName.toLowerCase();
  if (parentTagName) {
    return tagNames.some(function (name) {
      return parentTagName === name;
    });
  }
  return false;
}

/*
*   addNodes: Use targetList to generate nodeList of elements and to
*   each of these, add an overlay with a unique CSS class name.
*   Optionally, if getInfo is specified, add tooltip information;
*   if dndFlag is set, add drag-and-drop functionality.
*/
function addNodes(params) {
  var targetList = params.targetList,
      cssClass = params.cssClass,
      getInfo = params.getInfo,
      evalInfo = params.evalInfo,
      dndFlag = params.dndFlag;
  var counter = 0;

  targetList.forEach(function (target) {
    // Collect elements based on selector defined for target
    var elements = document.querySelectorAll(target.selector);

    // Filter elements if target defines a filter function
    if (typeof target.filter === 'function') elements = Array.prototype.filter.call(elements, target.filter);

    Array.prototype.forEach.call(elements, function (element) {
      if (isVisible(element)) {
        var info = getInfo(element, target);
        if (evalInfo) evalInfo(info, target);
        var boundingRect = element.getBoundingClientRect();
        var overlayNode = createOverlay(target, boundingRect, cssClass);
        if (dndFlag) addDragAndDrop(overlayNode);
        var labelNode = overlayNode.firstChild;
        labelNode.title = formatInfo(info);
        document.body.appendChild(overlayNode);
        counter += 1;
      }
    });
  });

  return counter;
}

/*
*   removeNodes: Use the unique CSS class name supplied to addNodes
*   to remove all instances of the overlay nodes.
*/
function removeNodes(cssClass) {
  var selector = "div." + cssClass;
  var elements = document.querySelectorAll(selector);
  Array.prototype.forEach.call(elements, function (element) {
    document.body.removeChild(element);
  });
}

/*
*   embedded.js
*/

// LOW-LEVEL FUNCTIONS

/*
*   getInputValue: Get current value of 'input' or 'textarea' element.
*/
function getInputValue(element) {
  return normalize(element.value);
}

/*
*   getRangeValue: Get current value of control with role 'spinbutton'
*   or 'slider' (i.e., subclass of abstract 'range' role).
*/
function getRangeValue(element) {
  var value;

  value = getAttributeValue(element, 'aria-valuetext');
  if (value.length) return value;

  value = getAttributeValue(element, 'aria-valuenow');
  if (value.length) return value;

  return getInputValue(element);
}

// HELPER FUNCTIONS FOR SPECIFIC ROLES

function getTextboxValue(element) {
  var inputTypes = ['email', 'password', 'search', 'tel', 'text', 'url'],
      tagName = element.tagName.toLowerCase(),
      type = element.type;

  if (tagName === 'input' && inputTypes.indexOf(type) !== -1) {
    return getInputValue(element);
  }

  if (tagName === 'textarea') {
    return getInputValue(element);
  }

  return '';
}

function getComboboxValue(element) {
  var inputTypes = ['email', 'search', 'tel', 'text', 'url'],
      tagName = element.tagName.toLowerCase(),
      type = element.type;

  if (tagName === 'input' && inputTypes.indexOf(type) !== -1) {
    return getInputValue(element);
  }

  return '';
}

function getSliderValue(element) {
  var tagName = element.tagName.toLowerCase(),
      type = element.type;

  if (tagName === 'input' && type === 'range') {
    return getRangeValue(element);
  }

  return '';
}

function getSpinbuttonValue(element) {
  var tagName = element.tagName.toLowerCase(),
      type = element.type;

  if (tagName === 'input' && type === 'number') {
    return getRangeValue(element);
  }

  return '';
}

function getListboxValue(element) {
  var tagName = element.tagName.toLowerCase();

  if (tagName === 'select') {
    var arr = [],
        selectedOptions = element.selectedOptions;

    for (var i = 0; i < selectedOptions.length; i++) {
      var option = selectedOptions[i];
      var value = normalize(option.value);
      if (value.length) arr.push(value);
    }

    if (arr.length) return arr.join(' ');
  }

  return '';
}

/*
*   isEmbeddedControl: Determine whether element has a role that corresponds
*   to an HTML form control that could be embedded within text content.
*/
function isEmbeddedControl(element) {
  var embeddedControlRoles = ['textbox', 'combobox', 'listbox', 'slider', 'spinbutton'];
  var role = getAriaRole(element);

  return embeddedControlRoles.indexOf(role) !== -1;
}

/*
*   getEmbeddedControlValue: Based on the role of element, use native semantics
*   of HTML to get the corresponding text value of the embedded control.
*/
function getEmbeddedControlValue(element) {
  var role = getAriaRole(element);

  switch (role) {
    case 'textbox':
      return getTextboxValue(element);

    case 'combobox':
      return getComboboxValue(element);

    case 'listbox':
      return getListboxValue(element);

    case 'slider':
      return getSliderValue(element);

    case 'spinbutton':
      return getSpinbuttonValue(element);
  }

  return '';
}

/*
*   getaccname.js
*
*   Note: Information in this module is based on the following documents:
*   1. HTML Accessibility API Mappings (latest editors draft) (http://rawgit.com/w3c/aria/master/html-aam/html-aam.html)
*   2. SVG Accessibility API Mappings (http://rawgit.com/w3c/aria/master/svg-aam/svg-aam.html)
*/

/*
*   getFieldsetLegendLabels: Recursively collect legend contents of
*   fieldset ancestors, starting with the closest (innermost).
*   Return collection as a possibly empty array of strings.
*/
function getFieldsetLegendLabels(element) {
  var arrayOfStrings = [];

  if (typeof element.closest !== 'function') {
    return arrayOfStrings;
  }

  function getLabelsRec(elem, arr) {
    var fieldset = elem.closest('fieldset');

    if (fieldset) {
      var legend = fieldset.querySelector('legend');
      if (legend) {
        var text = getElementContents(legend);
        if (text.length) {
          arr.push({ name: text, source: 'fieldset/legend' });
        }
      }
      // process ancestors
      getLabelsRec(fieldset.parentNode, arr);
    }
  }

  getLabelsRec(element, arrayOfStrings);
  return arrayOfStrings;
}

/*
*   getGroupingLabels: Return an array of grouping label objects for
*   element, each with two properties: 'name' and 'source'.
*/
function getGroupingLabels(element) {
  // We currently only handle labelable elements as defined in HTML 5.1
  if (isLabelableElement(element)) {
    return getFieldsetLegendLabels(element);
  }

  return [];
}

/*
*   nameFromNativeSemantics: Use method appropriate to the native semantics
*   of element to find accessible name. Includes methods for all interactive
*   elements. For non-interactive elements, if the element's ARIA role allows
*   its acc. name to be derived from its text contents, or if recFlag is set,
*   indicating that we are in a recursive aria-labelledby calculation, the
*   nameFromContents method is used.
*/
function nameFromNativeSemantics(element, recFlag) {
  var tagName = element.tagName.toLowerCase(),
      ariaRole = getAriaRole(element),
      accName = null;

  // TODO: Verify that this applies to all elements
  if (ariaRole && (ariaRole === 'presentation' || ariaRole === 'none')) {
    return null;
  }

  switch (tagName) {
    // FORM ELEMENTS: INPUT
    case 'input':
      switch (element.type) {
        // HIDDEN
        case 'hidden':
          if (recFlag) {
            accName = nameFromLabelElement(element);
          }
          break;

        // TEXT FIELDS
        case 'email':
        case 'password':
        case 'search':
        case 'tel':
        case 'text':
        case 'url':
          accName = nameFromLabelElement(element);
          if (accName === null) accName = nameFromAttribute(element, 'placeholder');
          break;

        // OTHER INPUT TYPES
        case 'button':
          accName = nameFromAttribute(element, 'value');
          break;

        case 'reset':
          accName = nameFromAttribute(element, 'value');
          if (accName === null) accName = nameFromDefault('Reset');
          break;

        case 'submit':
          accName = nameFromAttribute(element, 'value');
          if (accName === null) accName = nameFromDefault('Submit');
          break;

        case 'image':
          accName = nameFromAltAttribute(element);
          if (accName === null) accName = nameFromAttribute(element, 'value');
          break;

        default:
          accName = nameFromLabelElement(element);
          break;
      }
      break;

    // FORM ELEMENTS: OTHER
    case 'button':
      accName = nameFromContents(element);
      break;

    case 'label':
      accName = nameFromContents(element);
      break;

    case 'keygen':
    case 'meter':
    case 'output':
    case 'progress':
    case 'select':
      accName = nameFromLabelElement(element);
      break;

    case 'textarea':
      accName = nameFromLabelElement(element);
      if (accName === null) accName = nameFromAttribute(element, 'placeholder');
      break;

    // EMBEDDED ELEMENTS
    case 'audio':
      // if 'controls' attribute is present
      accName = { name: 'NOT YET IMPLEMENTED', source: '' };
      break;

    case 'embed':
      accName = { name: 'NOT YET IMPLEMENTED', source: '' };
      break;

    case 'iframe':
      accName = nameFromAttribute(element, 'title');
      break;

    case 'img':
    case 'area':
      // added
      accName = nameFromAltAttribute(element);
      break;

    case 'object':
      accName = { name: 'NOT YET IMPLEMENTED', source: '' };
      break;

    case 'svg':
      // added
      accName = nameFromDescendant(element, 'title');
      break;

    case 'video':
      // if 'controls' attribute is present
      accName = { name: 'NOT YET IMPLEMENTED', source: '' };
      break;

    // OTHER ELEMENTS
    case 'a':
      accName = nameFromContents(element);
      break;

    case 'details':
      accName = nameFromDetailsOrSummary(element);
      break;

    case 'figure':
      accName = nameFromDescendant(element, 'figcaption');
      break;

    case 'table':
      accName = nameFromDescendant(element, 'caption');
      break;

    // ELEMENTS NOT SPECIFIED ABOVE
    default:
      if (nameFromIncludesContents(element) || recFlag) accName = nameFromContents(element);
      break;
  }

  // LAST RESORT USE TITLE
  if (accName === null) accName = nameFromAttribute(element, 'title');

  return accName;
}

/*
*   nameFromAttributeIdRefs: Get the value of attrName on element (a space-
*   separated list of IDREFs), visit each referenced element in the order it
*   appears in the list and obtain its accessible name (skipping recursive
*   aria-labelledby or aria-describedby calculations), and return an object
*   with name property set to a string that is a space-separated concatenation
*   of those results if any, otherwise return null.
*/
function nameFromAttributeIdRefs(element, attribute) {
  var value = getAttributeValue(element, attribute);
  var idRefs,
      refElement,
      accName,
      arr = [];

  if (value.length) {
    idRefs = value.split(' ');

    for (var i = 0; i < idRefs.length; i++) {
      refElement = document.getElementById(idRefs[i]);
      if (refElement) {
        accName = getAccessibleName(refElement, true);
        if (accName && accName.name.length) arr.push(accName.name);
      }
    }
  }

  if (arr.length) return { name: arr.join(' '), source: attribute };

  return null;
}

/*
*   getAccessibleName: Use the ARIA Roles Model specification for accessible
*   name calculation based on its precedence order:
*   (1) Use aria-labelledby, unless a traversal is already underway;
*   (2) Use aria-label attribute value;
*   (3) Use whatever method is specified by the native semantics of the
*   element, which includes, as last resort, use of the title attribute.
*/
function getAccessibleName(element, recFlag) {
  var accName = null;

  if (!recFlag) accName = nameFromAttributeIdRefs(element, 'aria-labelledby');
  if (accName === null) accName = nameFromAttribute(element, 'aria-label');
  if (accName === null) accName = nameFromNativeSemantics(element, recFlag);

  return accName;
}

/*
*   getAccessibleDesc: Use the ARIA Roles Model specification for accessible
*   description calculation based on its precedence order:
*   (1) Use aria-describedby, unless a traversal is already underway;
*   (2) As last resort, use the title attribute.
*/
function getAccessibleDesc(element, recFlag) {
  var accDesc = null;

  if (!recFlag) accDesc = nameFromAttributeIdRefs(element, 'aria-describedby');
  if (accDesc === null) accDesc = nameFromAttribute(element, 'title');

  return accDesc;
}

/*
*   info.js: Function for displaying information on highlighted elements
*/

/*
*   getElementInfo: Extract tagName and other attribute information
*   based on tagName and return as formatted string.
*/
function getElementInfo(element) {
  var tagName = element.tagName.toLowerCase(),
      elementInfo = tagName;

  if (tagName === 'input') {
    var type = element.type;
    if (type && type.length) elementInfo += ' [type="' + type + '"]';
  }

  if (tagName === 'label') {
    var forVal = getAttributeValue(element, 'for');
    if (forVal.length) elementInfo += ' [for="' + forVal + '"]';
  }

  if (isLabelableElement(element)) {
    var id = element.id;
    if (id && id.length) elementInfo += ' [id="' + id + '"]';
  }

  if (element.hasAttribute('role')) {
    var role = getAttributeValue(element, 'role');
    if (role.length) elementInfo += ' [role="' + role + '"]';
  }

  return elementInfo;
}

/*
*   formatInfo: Convert info properties into a string with line breaks.
*/
function formatInfo(info) {
  var value = '';
  var title = info.title,
      element = info.element,
      grpLabels = info.grpLabels,
      accName = info.accName,
      accDesc = info.accDesc,
      role = info.role,
      props = info.props;

  value += '=== ' + title + ' ===';

  if (element) value += '\nELEMENT: ' + element;

  if (grpLabels && grpLabels.length) {
    // array starts with innermost label, so process from the end
    for (var i = grpLabels.length - 1; i >= 0; i--) {
      value += '\nGRP. LABEL: ' + grpLabels[i].name + '\nFROM: ' + grpLabels[i].source;
    }
  }

  if (accName) {
    value += '\nACC. NAME: ' + accName.name + '\nFROM: ' + accName.source;
  }

  if (accDesc) {
    value += '\nACC. DESC: ' + accDesc.name + '\nFROM: ' + accDesc.source;
  }

  if (role) value += '\nROLE: ' + role;

  if (props) value += '\nPROPERTIES: ' + props;

  return value;
}

/*
*   namefrom.js
*/

// LOW-LEVEL FUNCTIONS

/*
*   normalize: Trim leading and trailing whitespace and condense all
*   internal sequences of whitespace to a single space. Adapted from
*   Mozilla documentation on String.prototype.trim polyfill. Handles
*   BOM and NBSP characters.
*/
function normalize(s) {
  var rtrim = /^[\s\uFEFF\xA0]+|[\s\uFEFF\xA0]+$/g;
  return s.replace(rtrim, '').replace(/\s+/g, ' ');
}

/*
*   getAttributeValue: Return attribute value if present on element,
*   otherwise return empty string.
*/
function getAttributeValue(element, attribute) {
  var value = element.getAttribute(attribute);
  return value === null ? '' : normalize(value);
}

/*
*   couldHaveAltText: Based on HTML5 specification, determine whether
*   element could have an 'alt' attribute.
*/
function couldHaveAltText(element) {
  var tagName = element.tagName.toLowerCase();

  switch (tagName) {
    case 'img':
    case 'area':
      return true;
    case 'input':
      return element.type && element.type === 'image';
  }

  return false;
}

/*
*   hasEmptyAltText: Determine whether the alt attribute is present
*   and its value is the empty string.
*/
function hasEmptyAltText(element) {
  var value = element.getAttribute('alt');

  // Attribute is present
  if (value !== null) return normalize(value).length === 0;

  return false;
}

/*
*   isLabelableElement: Based on HTML5 specification, determine whether
*   element can be associated with a label.
*/
function isLabelableElement(element) {
  var tagName = element.tagName.toLowerCase(),
      type = element.type;

  switch (tagName) {
    case 'input':
      return type !== 'hidden';
    case 'button':
    case 'keygen':
    case 'meter':
    case 'output':
    case 'progress':
    case 'select':
    case 'textarea':
      return true;
    default:
      return false;
  }
}

/*
*   addCssGeneratedContent: Add CSS-generated content for pseudo-elements
*   :before and :after. According to the CSS spec, test that content value
*   is other than the default computed value of 'none'.
*
*   Note: Even if an author specifies content: 'none', because browsers add
*   the double-quote character to the beginning and end of computed string
*   values, the result cannot and will not be equal to 'none'.
*/
function addCssGeneratedContent(element, contents) {
  var result = contents,
      prefix = getComputedStyle(element, ':before').content,
      suffix = getComputedStyle(element, ':after').content;

  if (prefix !== 'none') result = prefix + result;
  if (suffix !== 'none') result = result + suffix;

  return result;
}

/*
*   getNodeContents: Recursively process element and text nodes by aggregating
*   their text values for an ARIA text equivalent calculation.
*   1. This includes special handling of elements with 'alt' text and embedded
*      controls.
*   2. The forElem parameter is needed for label processing to avoid inclusion
*      of an embedded control's value when the label is for the control itself.
*/
function getNodeContents(node, forElem) {
  var contents = '';

  if (node === forElem) return '';

  switch (node.nodeType) {
    case Node.ELEMENT_NODE:
      if (couldHaveAltText(node)) {
        contents = getAttributeValue(node, 'alt');
      } else if (isEmbeddedControl(node)) {
        contents = getEmbeddedControlValue(node);
      } else {
        if (node.hasChildNodes()) {
          var children = node.childNodes,
              arr = [];

          for (var i = 0; i < children.length; i++) {
            var nc = getNodeContents(children[i], forElem);
            if (nc.length) arr.push(nc);
          }

          contents = arr.length ? arr.join(' ') : '';
        }
      }
      // For all branches of the ELEMENT_NODE case...
      contents = addCssGeneratedContent(node, contents);
      break;

    case Node.TEXT_NODE:
      contents = normalize(node.textContent);
  }

  return contents;
}

/*
*   getElementContents: Construct the ARIA text alternative for element by
*   processing its element and text node descendants and then adding any CSS-
*   generated content if present.
*/
function getElementContents(element, forElement) {
  var result = '';

  if (element.hasChildNodes()) {
    var children = element.childNodes,
        arrayOfStrings = [];

    for (var i = 0; i < children.length; i++) {
      var contents = getNodeContents(children[i], forElement);
      if (contents.length) arrayOfStrings.push(contents);
    }

    result = arrayOfStrings.length ? arrayOfStrings.join(' ') : '';
  }

  return addCssGeneratedContent(element, result);
}

/*
*   getContentsOfChildNodes: Using predicate function for filtering element
*   nodes, collect text content from all child nodes of element.
*/
function getContentsOfChildNodes(element, predicate) {
  var arr = [],
      content;

  Array.prototype.forEach.call(element.childNodes, function (node) {
    switch (node.nodeType) {
      case Node.ELEMENT_NODE:
        if (predicate(node)) {
          content = getElementContents(node);
          if (content.length) arr.push(content);
        }
        break;
      case Node.TEXT_NODE:
        content = normalize(node.textContent);
        if (content.length) arr.push(content);
        break;
    }
  });

  if (arr.length) return arr.join(' ');
  return '';
}

// HIGHER-LEVEL FUNCTIONS THAT RETURN AN OBJECT WITH SOURCE PROPERTY

/*
*   nameFromAttribute
*/
function nameFromAttribute(element, attribute) {
  var name;

  name = getAttributeValue(element, attribute);
  if (name.length) return { name: name, source: attribute };

  return null;
}

/*
*   nameFromAltAttribute
*/
function nameFromAltAttribute(element) {
  var name = element.getAttribute('alt');

  // Attribute is present
  if (name !== null) {
    name = normalize(name);
    return name.length ? { name: name, source: 'alt' } : { name: '<empty>', source: 'alt' };
  }

  // Attribute not present
  return null;
}

/*
*   nameFromContents
*/
function nameFromContents(element) {
  var name;

  name = getElementContents(element);
  if (name.length) return { name: name, source: 'contents' };

  return null;
}

/*
*   nameFromDefault
*/
function nameFromDefault(name) {
  return name.length ? { name: name, source: 'default' } : null;
}

/*
*   nameFromDescendant
*/
function nameFromDescendant(element, tagName) {
  var descendant = element.querySelector(tagName);
  if (descendant) {
    var name = getElementContents(descendant);
    if (name.length) return { name: name, source: tagName + ' element' };
  }

  return null;
}

/*
*   nameFromLabelElement
*/
function nameFromLabelElement(element) {
  var name, label;

  // label [for=id]
  if (element.id) {
    label = document.querySelector('[for="' + element.id + '"]');
    if (label) {
      name = getElementContents(label, element);
      if (name.length) return { name: name, source: 'label reference' };
    }
  }

  // label encapsulation
  if (typeof element.closest === 'function') {
    label = element.closest('label');
    if (label) {
      name = getElementContents(label, element);
      if (name.length) return { name: name, source: 'label encapsulation' };
    }
  }

  return null;
}

/*
*   nameFromDetailsOrSummary: If element is expanded (has open attribute),
*   return the contents of the summary element followed by the text contents
*   of element and all of its non-summary child elements. Otherwise, return
*   only the contents of the first summary element descendant.
*/
function nameFromDetailsOrSummary(element) {
  var name, summary;

  function isExpanded(elem) {
    return elem.hasAttribute('open');
  }

  // At minimum, always use summary contents
  summary = element.querySelector('summary');
  if (summary) name = getElementContents(summary);

  // Return either summary + details (non-summary) or summary only
  if (isExpanded(element)) {
    name += getContentsOfChildNodes(element, function (elem) {
      return elem.tagName.toLowerCase() !== 'summary';
    });
    if (name.length) return { name: name, source: 'contents' };
  } else {
    if (name.length) return { name: name, source: 'summary element' };
  }

  return null;
}

/*
*   overlay.js: functions for creating and modifying DOM overlay elements
*/

var zIndex = 100000;

/*
*   createOverlay: Create overlay div with size and position based on the
*   boundingRect properties of its corresponding target element.
*/
function createOverlay(tgt, rect, cname) {
  var scrollOffsets = getScrollOffsets();
  var MINWIDTH = 68;
  var MINHEIGHT = 27;

  var node = document.createElement("div");
  node.setAttribute("class", [cname, 'oaa-element-overlay'].join(' '));
  node.startLeft = rect.left + scrollOffsets.x + "px";
  node.startTop = rect.top + scrollOffsets.y + "px";

  node.style.left = node.startLeft;
  node.style.top = node.startTop;
  node.style.width = Math.max(rect.width, MINWIDTH) + "px";
  node.style.height = Math.max(rect.height, MINHEIGHT) + "px";
  node.style.borderColor = tgt.color;
  node.style.zIndex = zIndex;

  var label = document.createElement("div");
  label.setAttribute("class", 'oaa-overlay-label');
  label.style.backgroundColor = tgt.color;
  label.innerHTML = tgt.label;

  node.appendChild(label);
  return node;
}

/*
*   addDragAndDrop: Add drag-and-drop and reposition functionality to an
*   overlay div element created by the createOverlay function.
*/
function addDragAndDrop(node) {

  function hoistZIndex(el) {
    var incr = 100;
    el.style.zIndex = zIndex += incr;
  }

  function repositionOverlay(el) {
    el.style.left = el.startLeft;
    el.style.top = el.startTop;
  }

  var labelDiv = node.firstChild;

  labelDiv.onmousedown = function (event) {
    drag(this.parentNode, hoistZIndex, event);
  };

  labelDiv.ondblclick = function (event) {
    repositionOverlay(this.parentNode);
  };
}

/*
*   roles.js
*
*   Note: The information in this module is based on the following documents:
*   1. ARIA in HTML (https://specs.webplatform.org/html-aria/webspecs/master/)
*   2. WAI-ARIA 1.1 (http://www.w3.org/TR/wai-aria-1.1/)
*   3. WAI-ARIA 1.0 (http://www.w3.org/TR/wai-aria/)
*/

/*
*   inListOfOptions: Determine whether element is a child of
*   1. a select element
*   2. an optgroup element that is a child of a select element
*   3. a datalist element
*/
function inListOfOptions(element) {
  var parent = element.parentElement,
      parentName = parent.tagName.toLowerCase(),
      parentOfParentName = parent.parentElement.tagName.toLowerCase();

  if (parentName === 'select') return true;

  if (parentName === 'optgroup' && parentOfParentName === 'select') return true;

  if (parentName === 'datalist') return true;

  return false;
}

/*
*   validRoles: Reference list of all concrete ARIA roles as specified in
*   WAI-ARIA 1.1 Working Draft of 14 July 2015
*/
var validRoles = [

// LANDMARK
'application', 'banner', 'complementary', 'contentinfo', 'form', 'main', 'navigation', 'search',

// WIDGET
'alert', 'alertdialog', 'button', 'checkbox', 'dialog', 'gridcell', 'link', 'log', 'marquee', 'menuitem', 'menuitemcheckbox', 'menuitemradio', 'option', 'progressbar', 'radio', 'scrollbar', 'searchbox', // ARIA 1.1
'slider', 'spinbutton', 'status', 'switch', // ARIA 1.1
'tab', 'tabpanel', 'textbox', 'timer', 'tooltip', 'treeitem',

// COMPOSITE WIDGET
'combobox', 'grid', 'listbox', 'menu', 'menubar', 'radiogroup', 'tablist', 'tree', 'treegrid',

// DOCUMENT STRUCTURE
'article', 'cell', // ARIA 1.1
'columnheader', 'definition', 'directory', 'document', 'group', 'heading', 'img', 'list', 'listitem', 'math', 'none', // ARIA 1.1
'note', 'presentation', 'region', 'row', 'rowgroup', 'rowheader', 'separator', 'table', // ARIA 1.1
'text', // ARIA 1.1
'toolbar'];

/*
*   getValidRole: Examine each value in space-separated list by attempting
*   to find its match in the validRoles array. If a match is found, return
*   it. Otherwise, return null.
*/
function getValidRole(spaceSepList) {
  var arr = spaceSepList.split(' ');

  for (var i = 0; i < arr.length; i++) {
    var value = arr[i].toLowerCase();
    var validRole = validRoles.find(function (role) {
      return role === value;
    });
    if (validRole) return validRole;
  }

  return null;
}

/*
*   getAriaRole: Get the value of the role attribute, if it is present. If
*   not specified, get the default role of element if it has one. Based on
*   ARIA in HTML as of 21 October 2015.
*/
function getAriaRole(element) {
  var tagName = element.tagName.toLowerCase(),
      type = element.type;

  if (element.hasAttribute('role')) {
    return getValidRole(getAttributeValue(element, 'role'));
  }

  switch (tagName) {

    case 'a':
      if (element.hasAttribute('href')) return 'link';
      break;

    case 'area':
      if (element.hasAttribute('href')) return 'link';
      break;

    case 'article':
      return 'article';
    case 'aside':
      return 'complementary';
    case 'body':
      return 'document';
    case 'button':
      return 'button';
    case 'datalist':
      return 'listbox';
    case 'details':
      return 'group';
    case 'dialog':
      return 'dialog';
    case 'dl':
      return 'list';
    case 'fieldset':
      return 'group';

    case 'footer':
      if (!isDescendantOf(element, ['article', 'section'])) return 'contentinfo';
      break;

    case 'form':
      return 'form';

    case 'h1':
      return 'heading';
    case 'h2':
      return 'heading';
    case 'h3':
      return 'heading';
    case 'h4':
      return 'heading';
    case 'h5':
      return 'heading';
    case 'h6':
      return 'heading';

    case 'header':
      if (!isDescendantOf(element, ['article', 'section'])) return 'banner';
      break;

    case 'hr':
      return 'separator';

    case 'img':
      if (!hasEmptyAltText(element)) return 'img';
      break;

    case 'input':
      if (type === 'button') return 'button';
      if (type === 'checkbox') return 'checkbox';
      if (type === 'email') return element.hasAttribute('list') ? 'combobox' : 'textbox';
      if (type === 'image') return 'button';
      if (type === 'number') return 'spinbutton';
      if (type === 'password') return 'textbox';
      if (type === 'radio') return 'radio';
      if (type === 'range') return 'slider';
      if (type === 'reset') return 'button';
      if (type === 'search') return element.hasAttribute('list') ? 'combobox' : 'textbox';
      if (type === 'submit') return 'button';
      if (type === 'tel') return element.hasAttribute('list') ? 'combobox' : 'textbox';
      if (type === 'text') return element.hasAttribute('list') ? 'combobox' : 'textbox';
      if (type === 'url') return element.hasAttribute('list') ? 'combobox' : 'textbox';
      break;

    case 'li':
      if (hasParentWithName(element, ['ol', 'ul'])) return 'listitem';
      break;

    case 'link':
      if (element.hasAttribute('href')) return 'link';
      break;

    case 'main':
      return 'main';

    case 'menu':
      if (type === 'toolbar') return 'toolbar';
      break;

    case 'menuitem':
      if (type === 'command') return 'menuitem';
      if (type === 'checkbox') return 'menuitemcheckbox';
      if (type === 'radio') return 'menuitemradio';
      break;

    case 'meter':
      return 'progressbar';
    case 'nav':
      return 'navigation';
    case 'ol':
      return 'list';

    case 'option':
      if (inListOfOptions(element)) return 'option';
      break;

    case 'output':
      return 'status';
    case 'progress':
      return 'progressbar';
    case 'section':
      return 'region';
    case 'select':
      return 'listbox';
    case 'summary':
      return 'button';

    case 'tbody':
      return 'rowgroup';
    case 'tfoot':
      return 'rowgroup';
    case 'thead':
      return 'rowgroup';

    case 'textarea':
      return 'textbox';

    // TODO: th can have role 'columnheader' or 'rowheader'
    case 'th':
      return 'columnheader';

    case 'ul':
      return 'list';
  }

  return null;
}

/*
*   nameFromIncludesContents: Determine whether the ARIA role of element
*   specifies that its 'name from' includes 'contents'.
*/
function nameFromIncludesContents(element) {
  var elementRole = getAriaRole(element);
  if (elementRole === null) return false;

  var contentsRoles = ['button', 'cell', // ARIA 1.1
  'checkbox', 'columnheader', 'directory', 'gridcell', 'heading', 'link', 'listitem', 'menuitem', 'menuitemcheckbox', 'menuitemradio', 'option', 'radio', 'row', 'rowgroup', 'rowheader', 'switch', // ARIA 1.1
  'tab', 'text', // ARIA 1.1
  'tooltip', 'treeitem'];

  var contentsRole = contentsRoles.find(function (role) {
    return role === elementRole;
  });
  return typeof contentsRole !== 'undefined';
}

/*
*   utils.js: utility functions
*/

/*
*   getScrollOffsets: Use x and y scroll offsets to calculate positioning
*   coordinates that take into account whether the page has been scrolled.
*   From Mozilla Developer Network: Element.getBoundingClientRect()
*/
function getScrollOffsets() {
  var t;

  var xOffset = typeof window.pageXOffset === "undefined" ? (((t = document.documentElement) || (t = document.body.parentNode)) && typeof t.ScrollLeft === 'number' ? t : document.body).ScrollLeft : window.pageXOffset;

  var yOffset = typeof window.pageYOffset === "undefined" ? (((t = document.documentElement) || (t = document.body.parentNode)) && typeof t.ScrollTop === 'number' ? t : document.body).ScrollTop : window.pageYOffset;

  return { x: xOffset, y: yOffset };
}

/*
*   drag: Add drag and drop functionality to an element by setting this
*   as its mousedown handler. Depends upon getScrollOffsets function.
*   From JavaScript: The Definitive Guide, 6th Edition (slightly modified)
*/
function drag(elementToDrag, dragCallback, event) {
  var scroll = getScrollOffsets();
  var startX = event.clientX + scroll.x;
  var startY = event.clientY + scroll.y;

  var origX = elementToDrag.offsetLeft;
  var origY = elementToDrag.offsetTop;

  var deltaX = startX - origX;
  var deltaY = startY - origY;

  if (dragCallback) dragCallback(elementToDrag);

  if (document.addEventListener) {
    document.addEventListener("mousemove", moveHandler, true);
    document.addEventListener("mouseup", upHandler, true);
  } else if (document.attachEvent) {
    elementToDrag.setCapture();
    elementToDrag.attachEvent("onmousemove", moveHandler);
    elementToDrag.attachEvent("onmouseup", upHandler);
    elementToDrag.attachEvent("onlosecapture", upHandler);
  }

  if (event.stopPropagation) event.stopPropagation();else event.cancelBubble = true;

  if (event.preventDefault) event.preventDefault();else event.returnValue = false;

  function moveHandler(e) {
    if (!e) e = window.event;

    var scroll = getScrollOffsets();
    elementToDrag.style.left = e.clientX + scroll.x - deltaX + "px";
    elementToDrag.style.top = e.clientY + scroll.y - deltaY + "px";

    elementToDrag.style.cursor = "move";

    if (e.stopPropagation) e.stopPropagation();else e.cancelBubble = true;
  }

  function upHandler(e) {
    if (!e) e = window.event;

    elementToDrag.style.cursor = "grab";
    elementToDrag.style.cursor = "-moz-grab";
    elementToDrag.style.cursor = "-webkit-grab";

    if (document.removeEventListener) {
      document.removeEventListener("mouseup", upHandler, true);
      document.removeEventListener("mousemove", moveHandler, true);
    } else if (document.detachEvent) {
      elementToDrag.detachEvent("onlosecapture", upHandler);
      elementToDrag.detachEvent("onmouseup", upHandler);
      elementToDrag.detachEvent("onmousemove", moveHandler);
      elementToDrag.releaseCapture();
    }

    if (e.stopPropagation) e.stopPropagation();else e.cancelBubble = true;
  }
}

/*
*   addPolyfills: Add polyfill implementations for JavaScript object methods
*   defined in ES6 and used by bookmarklets:
*   1. Array.prototype.find
*   2. String.prototype.includes
*/
function addPolyfills() {

  // From https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/find
  if (!Array.prototype.find) {
    Array.prototype.find = function (predicate) {
      if (this === null) {
        throw new TypeError('Array.prototype.find called on null or undefined');
      }
      if (typeof predicate !== 'function') {
        throw new TypeError('predicate must be a function');
      }
      var list = Object(this);
      var length = list.length >>> 0;
      var thisArg = arguments[1];
      var value;

      for (var i = 0; i < length; i++) {
        value = list[i];
        if (predicate.call(thisArg, value, i, list)) {
          return value;
        }
      }
      return undefined;
    };
  }

  // From https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/includes
  if (!String.prototype.includes) {
    String.prototype.includes = function (search, start) {
      if (typeof start !== 'number') {
        start = 0;
      }

      if (start + search.length > this.length) {
        return false;
      } else {
        return this.indexOf(search, start) !== -1;
      }
    };
  }
}
