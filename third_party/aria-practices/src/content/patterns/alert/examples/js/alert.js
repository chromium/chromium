/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 */

'use strict';

window.addEventListener('load', function () {
  var button = document.getElementById('alert-trigger');

  button.addEventListener('click', addAlert);
});

/*
 * @function addAlert
 *
 * @desc Adds an alert to the page
 *
 * @param   {object}  event  -  Standard W3C event object
 *
 */

function addAlert() {
  var example = document.getElementById('example');
  var template = document.getElementById('alert-template').innerHTML;

  example.innerHTML = template;
}
