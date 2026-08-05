/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 */

/* global aria */

'use strict';

/**
 * ARIA Grid Examples
 *
 * @function onload
 * @description Initialize the grid examples once the page has loaded
 */

window.addEventListener('load', function () {
  // Initialize Example 1 Grid (if it is present in the DOM)
  var ex1GridElement = document.getElementById('ex1-grid');
  if (ex1GridElement) {
    new aria.Grid(ex1GridElement);
  }

  // Initialize Example 2 Grid (if it is present in the DOM)
  var ex2GridElement = document.getElementById('ex2-grid');
  if (ex2GridElement) {
    new aria.Grid(ex2GridElement);
  }

  // Initialize Example 3 Grid (if it is present in the DOM)
  var ex3GridElement = document.getElementById('ex3-grid');
  if (ex3GridElement) {
    var ex3Grid = new aria.Grid(ex3GridElement);
    var toggleButton = document.getElementById('toggle_column_btn');
    var toggledOn = true;

    toggleButton.addEventListener('click', function () {
      toggledOn = !toggledOn;

      ex3Grid.toggleColumn(2, toggledOn);
      ex3Grid.toggleColumn(4, toggledOn);

      if (toggledOn) {
        toggleButton.innerText = 'Hide Type and Category';
      } else {
        toggleButton.innerText = 'Show Type and Category';
      }
    });
  }
});
