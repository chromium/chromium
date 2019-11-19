// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.viewportSize = {width: 800, height: 600};
window.deviceScaleFactor = 1;
window.emulationScaleFactor = 1;
window.pageScaleFactor = 1;
window.pageZoomFactor = 1;
window.scrollX = 0;
window.scrollY = 0;

function reset(resetData) {
  window.viewportSize = resetData.viewportSize;
  window.deviceScaleFactor = resetData.deviceScaleFactor;
  window.pageScaleFactor = resetData.pageScaleFactor;
  window.pageZoomFactor = resetData.pageZoomFactor;
  window.emulationScaleFactor = resetData.emulationScaleFactor;
  window.scrollX = Math.round(resetData.scrollX);
  window.scrollY = Math.round(resetData.scrollY);

  window.canvas = document.getElementById('canvas');
  if (window.canvas) {
    window.canvas.width = deviceScaleFactor * viewportSize.width;
    window.canvas.height = deviceScaleFactor * viewportSize.height;
    window.canvas.style.width = viewportSize.width + 'px';
    window.canvas.style.height = viewportSize.height + 'px';

    window.context = canvas.getContext('2d');
    window.context.scale(deviceScaleFactor, deviceScaleFactor);

    window.canvasWidth = viewportSize.width;
    window.canvasHeight = viewportSize.height;
  }

  doReset();
}

function doReset() { }

function setPlatform(platform) {
  window.platform = platform;
  document.body.classList.add('platform-' + platform);
}

function dispatch(message) {
  const functionName = message.shift();
  window[functionName].apply(null, message);
}

function log(text) {
  let element = document.getElementById('log');
  if (!element) {
    element = document.body.createChild();
    element.id = 'log';
  }
  element.createChild('div').textContent = text;
}

function eventHasCtrlOrMeta(event) {
    return window.platform == 'mac' ? (event.metaKey && !event.ctrlKey) : (event.ctrlKey && !event.metaKey);
}

Element.prototype.createChild = function(tagName, className) {
  const element = createElement(tagName, className);
  element.addEventListener('click', function(e) { e.stopPropagation(); }, false);
  this.appendChild(element);
  return element;
}

Element.prototype.createTextChild = function(text) {
  const element = document.createTextNode(text);
  this.appendChild(element);
  return element;
}

Element.prototype.removeChildren = function()
{
  if (this.firstChild)
    this.textContent = '';
}

function createElement(tagName, className)
{
  const element = document.createElement(tagName);
  if (className)
    element.className = className;
  return element;
}

String.prototype.trimEnd = function(maxLength)
{
  if (this.length <= maxLength)
    return String(this);
  return this.substr(0, maxLength - 1) + '\u2026';
}

/**
 * @param {number} num
 * @param {number} min
 * @param {number} max
 * @return {number}
 */
Number.constrain = function(num, min, max) {
  if (num < min)
    num = min;
  else if (num > max)
    num = max;
  return num;
};
