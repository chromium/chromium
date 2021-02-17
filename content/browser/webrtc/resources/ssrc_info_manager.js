// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



/**
 * Get the ssrc if |report| is an ssrc report.
 *
 * @param {!Object} report The object contains id, type, and stats, where stats
 *     is the object containing timestamp and values, which is an array of
 *     strings, whose even index entry is the name of the stat, and the odd
 *     index entry is the value.
 * @return {?string} The ssrc.
 */
function GetSsrcFromReport(report) {
  if (report.type !== 'ssrc') {
    console.warn('Trying to get ssrc from non-ssrc report.');
    return null;
  }

  // If the 'ssrc' name-value pair exists, return the value; otherwise, return
  // the report id.
  // The 'ssrc' name-value pair only exists in an upcoming Libjingle change. Old
  // versions use id to refer to the ssrc.
  //
  // TODO(jiayl): remove the fallback to id once the Libjingle change is rolled
  // to Chrome.
  if (report.stats && report.stats.values) {
    for (var i = 0; i < report.stats.values.length - 1; i += 2) {
      if (report.stats.values[i] === 'ssrc') {
        return report.stats.values[i + 1];
      }
    }
  }
  return report.id;
}

/**
 * SsrcInfoManager stores the ssrc stream info extracted from SDP.
 */
var SsrcInfoManager = (function() {
  'use strict';

  /**
   * @constructor
   */
  function SsrcInfoManager() {
    /**
     * Map from ssrc id to an object containing all the stream properties.
     * @type {!Object<!Object<string>>}
     * @private
     */
    this.streamInfoContainer_ = {};

    /**
     * The string separating attibutes in an SDP.
     * @type {string}
     * @const
     * @private
     */
    this.ATTRIBUTE_SEPARATOR_ = /[\r,\n]/;

    /**
     * The regex separating fields within an ssrc description.
     * @type {RegExp}
     * @const
     * @private
     */
    this.FIELD_SEPARATOR_REGEX_ = / .*:/;

    /**
     * The prefix string of an ssrc description.
     * @type {string}
     * @const
     * @private
     */
    this.SSRC_ATTRIBUTE_PREFIX_ = 'a=ssrc:';

    /**
     * The className of the ssrc info parent element.
     * @type {string}
     * @const
     */
    this.SSRC_INFO_BLOCK_CLASS = 'ssrc-info-block';
  }

  SsrcInfoManager.prototype = {
    /**
     * Extracts the stream information from |sdp| and saves it.
     * For example:
     *     a=ssrc:1234 msid:abcd
     *     a=ssrc:1234 label:hello
     *
     * @param {string} sdp The SDP string.
     */
    addSsrcStreamInfo: function(sdp) {
      var attributes = sdp.split(this.ATTRIBUTE_SEPARATOR_);
      for (var i = 0; i < attributes.length; ++i) {
        // Check if this is a ssrc attribute.
        if (attributes[i].indexOf(this.SSRC_ATTRIBUTE_PREFIX_) !== 0) {
          continue;
        }

        var nextFieldIndex = attributes[i].search(this.FIELD_SEPARATOR_REGEX_);

        if (nextFieldIndex === -1) {
          continue;
        }

        var ssrc = attributes[i].substring(
            this.SSRC_ATTRIBUTE_PREFIX_.length, nextFieldIndex);
        if (!this.streamInfoContainer_[ssrc]) {
          this.streamInfoContainer_[ssrc] = {};
        }

        // Make |rest| starting at the next field.
        var rest = attributes[i].substring(nextFieldIndex + 1);
        var name, value;
        while (rest.length > 0) {
          nextFieldIndex = rest.search(this.FIELD_SEPARATOR_REGEX_);
          if (nextFieldIndex === -1) {
            nextFieldIndex = rest.length;
          }

          // The field name is the string before the colon.
          name = rest.substring(0, rest.indexOf(':'));
          // The field value is from after the colon to the next field.
          value = rest.substring(rest.indexOf(':') + 1, nextFieldIndex);
          this.streamInfoContainer_[ssrc][name] = value;

          // Move |rest| to the start of the next field.
          rest = rest.substring(nextFieldIndex + 1);
        }
      }
    },

    /**
     * @param {string} sdp The ssrc id.
     * @return {!Object<string>} The object containing the ssrc infomation.
     */
    getStreamInfo: function(ssrc) {
      return this.streamInfoContainer_[ssrc];
    },

    /**
     * Populate the ssrc information into |parentElement|, each field as a
     * DIV element.
     *
     * @param {!Element} parentElement The parent element for the ssrc info.
     * @param {string} ssrc The ssrc id.
     */
    populateSsrcInfo: function(parentElement, ssrc) {
      if (!this.streamInfoContainer_[ssrc]) {
        return;
      }

      parentElement.className = this.SSRC_INFO_BLOCK_CLASS;

      var fieldElement;
      for (var property in this.streamInfoContainer_[ssrc]) {
        fieldElement = document.createElement('div');
        parentElement.appendChild(fieldElement);
        fieldElement.textContent =
            property + ':' + this.streamInfoContainer_[ssrc][property];
      }
    }
  };

  return SsrcInfoManager;
})();
