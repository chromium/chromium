/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   Treeitem.js
 *
 *   Desc:   Setup click events for Tree widget examples
 */

'use strict';

/**
 * ARIA Treeview example
 *
 * @function onload
 * @description  after page has loaded initialize all treeitems based on the role=treeitem
 */

window.addEventListener('load', function () {
  var treeitems = document.querySelectorAll('[role="treeitem"]');

  for (var i = 0; i < treeitems.length; i++) {
    treeitems[i].addEventListener('click', function (event) {
      var treeitem = event.currentTarget;
      var label = treeitem.getAttribute('aria-label');
      if (!label) {
        var child = treeitem.firstElementChild;
        label = child ? child.innerText : treeitem.innerText;
      }

      document.getElementById('last_action').value = label.trim();

      event.stopPropagation();
      event.preventDefault();
    });
  }
});
