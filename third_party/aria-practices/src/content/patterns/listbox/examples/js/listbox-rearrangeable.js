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
 * ARIA Listbox Examples
 *
 * @function onload
 * @description Initialize the listbox examples once the page has loaded
 */

window.addEventListener('load', function () {
  // This onload handle initializes two examples. Only initialize example if the example
  // can be found in the dom.
  if (document.getElementById('ss_imp_list')) {
    var ex1ImportantListbox = new aria.Listbox(
      document.getElementById('ss_imp_list')
    );
    var ex1UnimportantListbox = new aria.Listbox(
      document.getElementById('ss_unimp_list')
    );
    new aria.Toolbar(document.querySelector('[role="toolbar"]'));

    ex1ImportantListbox.enableMoveUpDown(
      document.getElementById('ex1-up'),
      document.getElementById('ex1-down')
    );
    ex1ImportantListbox.setupMove(
      document.getElementById('ex1-delete'),
      ex1UnimportantListbox
    );
    ex1ImportantListbox.setHandleItemChange(function (event, items) {
      var updateText = '';

      switch (event) {
        case 'added':
          updateText =
            'Moved ' + items[0].innerText + ' to important features.';
          break;
        case 'removed':
          updateText =
            'Moved ' + items[0].innerText + ' to unimportant features.';
          break;
        case 'moved_up':
        case 'moved_down':
          var pos = Array.prototype.indexOf.call(
            this.listboxNode.children,
            items[0]
          );
          pos++;
          updateText = 'Moved to position ' + pos;
          break;
      }

      if (updateText) {
        var ex1LiveRegion = document.getElementById('ss_live_region');
        ex1LiveRegion.innerText = updateText;
      }
    });
    ex1UnimportantListbox.setupMove(
      document.getElementById('ex1-add'),
      ex1ImportantListbox
    );
  }

  // This onload handle initializes two examples. Only initialize example if the example
  // can be found in the dom.
  if (document.getElementById('ms_imp_list')) {
    var ex2ImportantListbox = new aria.Listbox(
      document.getElementById('ms_imp_list')
    );
    var ex2UnimportantListbox = new aria.Listbox(
      document.getElementById('ms_unimp_list')
    );

    ex2ImportantListbox.setupMove(
      document.getElementById('ex2-add'),
      ex2UnimportantListbox
    );
    ex2UnimportantListbox.setupMove(
      document.getElementById('ex2-delete'),
      ex2ImportantListbox
    );
    ex2UnimportantListbox.setHandleItemChange(function (event, items) {
      var updateText = '';
      var itemText = items.length === 1 ? '1 item' : items.length + ' items';

      switch (event) {
        case 'added':
          updateText = 'Added ' + itemText + ' to chosen features.';
          break;
        case 'removed':
          updateText = 'Removed ' + itemText + ' from chosen features.';
          break;
      }

      if (updateText) {
        var ex1LiveRegion = document.getElementById('ms_live_region');
        ex1LiveRegion.innerText = updateText;
      }
    });
  }
});
