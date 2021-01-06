// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function simulateClick(target) {
  simulateClickWithButton(target, 0 /* left click */);
}

function simulateMiddleClick(target) {
  simulateClickWithButton(target, 1 /* middle click */);
}

function simulateClickWithButton(target, button) {
  target = document.getElementById(target);
  var evt = new MouseEvent("click", {"button": button});
  return target.dispatchEvent(evt);
}

function createImpressionTag(id, url, data, destination) {
  createImpressionTagWithTarget(id, url, data, destination, "_top");
}

function createImpressionTagAtLocation(id, url, data, destination, left, top) {
  let anchor =
      createImpressionTagWithTarget(id, url, data, destination, "_top");
  const style =  "position: absolute; left: " + (left - 10) + "px; top: " +
      (top - 10) + "px; width: 20px; height: 20px;";
  anchor.setAttribute("style", style);
}

function createImpressionTagWithReportingAndExpiry(
    id, url, data, destination, report_origin, expiry) {
  let anchor = createImpressionTagWithTarget(
      id, url, data, destination, "_top");
  anchor.setAttribute("reportingorigin", report_origin);
  anchor.setAttribute("impressionexpiry", expiry);
}

function createImpressionTagWithReporting(
    id, url, data, destination, report_origin) {
  let anchor = createImpressionTagWithTarget(
      id, url, data, destination, "_top");
  anchor.setAttribute("reportingorigin", report_origin);
}

function createImpressionTagWithTarget(id, url, data, destination, target) {
  let anchor = document.createElement("a");
  anchor.href = url;
  anchor.setAttribute("impressiondata", data);
  anchor.setAttribute("conversiondestination", destination);
  anchor.setAttribute("target", target);
  anchor.width = 100;
  anchor.height = 100;
  anchor.id = id;

  // Create the text node for anchor element.
  var link = document.createTextNode("This is link");

  // Append the text node to anchor element.
  anchor.appendChild(link);
  document.body.appendChild(anchor);

  return anchor;
}
