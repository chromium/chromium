// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var iframeUpdateIntervalID;
var queryParams = '';

function selectConfig(el) {
  deselectAllItems('.config-item');
  selectItem(el);
  updateIframe();
}

function selectExample(el) {
  deselectAllItems('.nav-item');
  selectItem(el);
  updateIframe();
}

function updateIframe() {
  var exampleEl = document.querySelector('.nav-item.selected');
  var configEl = document.querySelector('.config-item.selected');
  var url = exampleEl.dataset.href + '?config=' + configEl.textContent;
  setIframeSrc(url);
}

function selectItem(el) {
  el.classList.add('selected');
}

function deselectAllItems(selector) {
  var navItemEls = document.querySelectorAll(selector);
  for (var i = 0; i < navItemEls.length; ++i) {
    navItemEls[i].classList.remove('selected');
  }
}

function setIframeSrc(src) {
  var iframeEl = document.querySelector('iframe');

  window.clearInterval(iframeUpdateIntervalID);
  iframeEl.style.height = '';
  iframeEl.src = src;
}

document.addEventListener('DOMContentLoaded', function () {
  var iframeEl = document.querySelector('iframe');
  var iframeWrapperEl = document.querySelector('.iframe-wrapper');

  var configItemEls = document.querySelectorAll('.config-item');
  for (var i = 0; i < configItemEls.length; ++i) {
    configItemEls[i].addEventListener('click', function (e) {
      selectConfig(this);
    });
  }

  var navItemEls = document.querySelectorAll('.nav-item');
  for (var i = 0; i < navItemEls.length; ++i) {
    navItemEls[i].addEventListener('click', function (e) {
      selectExample(this);
    });
  }

  iframeEl.addEventListener('load', function () {
    var iframeDocument = this.contentWindow.document;
    var iframeBodyEl = iframeDocument.body;
    iframeEl.style.height = iframeBodyEl.scrollHeight + 'px';

    // HACK: polling the body height to update the iframe. There's got to be a
    // better way to do this...
    var prevBodyHeight;
    var prevWrapperHeight;
    iframeUpdateIntervalID = window.setInterval(function () {
      var bodyHeight = iframeBodyEl.getBoundingClientRect().height;
      var wrapperHeight = iframeWrapperEl.clientHeight;
      if (bodyHeight != prevBodyHeight || wrapperHeight != prevWrapperHeight) {
        // HACK: magic 4... without it, the scrollbar is always visible. :(
        var newHeight = Math.max(wrapperHeight - 4, bodyHeight);
        iframeEl.style.height = newHeight + 'px';
        prevBodyHeight = bodyHeight;
        prevWrapperHeight = wrapperHeight;
      }
    }, 100);  // .1s
  }, false);

  var closeButtonEl = document.querySelector('.close-button');
  closeButtonEl.addEventListener('click', function () {
    window.close();
  });

  // select the first example.
  selectExample(document.querySelector('.nav-item'));
});
