'use strict';
// Copyright (C) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Picker used by <input type='datetime-local' />
 */

function initializeDateTimeLocalPicker(config) {
  const dateTimeLocalPicker = new DateTimeLocalPicker(config);
  global.picker = dateTimeLocalPicker;
  main.append(dateTimeLocalPicker);
  main.style.border = '1px solid transparent';
  main.style.height = dateTimeLocalPicker.height + 'px';
  main.style.width = dateTimeLocalPicker.width + 'px';
  resizeWindow(dateTimeLocalPicker.width + 2, dateTimeLocalPicker.height + 2);
}

/**
 * DateTimeLocalPicker: Custom element providing a datetime-local picker implementation.
 *             DateTimeLocalPicker contains 3 parts:
 *                 - date picker
 *                 - time picker
 *                 - submission controls
 */
class DateTimeLocalPicker extends HTMLElement {
  constructor(config) {
    super();

    this.className = DateTimeLocalPicker.ClassName;

    this.datePicker_ = new CalendarPicker(config.mode, config);
    this.timePicker_ = new TimePicker(config);
    this.append(this.datePicker_.element, this.timePicker_);

    this.submissionControls_ = new SubmissionControls(
        this.onSubmitButtonClick_, this.onCancelButtonClick_);
    this.append(this.submissionControls_);
    this.addEventListener('keydown', this.onKeyDown_);

    window.addEventListener('resize', this.onWindowResize_, {once: true});
  };

  onSubmitButtonClick_ = () => {
    const selectedValue = this.datePicker_.getSelectedValue() + 'T' +
        this.timePicker_.selectedValue;
    window.setTimeout(function() {
      window.pagePopupController.setValueAndClosePopup(0, selectedValue);
    }, 100);
  };

  onCancelButtonClick_ = () => {
    window.pagePopupController.closePopup();
  };

  onKeyDown_ = (event) => {
    switch (event.key) {
      case 'Enter':
        this.submissionControls_.submitButton.click();
        break;
      case 'Escape':
        this.submissionControls_.cancelButton.click();
        break;
    }
  };

  onWindowResize_ = (event) => {
    this.datePicker_.calendarTableView.element.focus();
  };

  get height() {
    return DateTimeLocalPicker.Height;
  }

  get width() {
    return this.datePicker_.width() + this.timePicker_.width;
  }

  get submissionControls() {
    return this.submissionControls_;
  }

  get datePicker() {
    return this.datePicker_;
  }

  get timePicker() {
    return this.timePicker_;
  }
}
DateTimeLocalPicker.ClassName = 'datetimelocal-picker';
DateTimeLocalPicker.Height = 320;
window.customElements.define('datetimelocal-picker', DateTimeLocalPicker);
