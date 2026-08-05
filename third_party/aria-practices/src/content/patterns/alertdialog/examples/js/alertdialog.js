'use strict';

var aria = aria || {};

aria.Utils = aria.Utils || {};

/**
 * @description Set focus on descendant nodes until the first focusable element is
 *       found.
 * @param element
 *          DOM node for which to find the first focusable descendant.
 * @returns {boolean}
 *  true if a focusable element is found and focus is set.
 */
aria.Utils.focusFirstDescendant = function (element) {
  for (var i = 0; i < element.childNodes.length; i++) {
    var child = element.childNodes[i];
    if (
      aria.Utils.attemptFocus(child) ||
      aria.Utils.focusFirstDescendant(child)
    ) {
      return true;
    }
  }
  return false;
}; // end focusFirstDescendant

/**
 * @description Set Attempt to set focus on the current node.
 * @param element
 *          The node to attempt to focus on.
 * @returns {boolean}
 *  true if element is focused.
 */
aria.Utils.attemptFocus = function (element) {
  if (!aria.Utils.isFocusable(element)) {
    return false;
  }

  aria.Utils.IgnoreUtilFocusChanges = true;
  try {
    element.focus();
  } catch (e) {
    // continue regardless of error
  }
  aria.Utils.IgnoreUtilFocusChanges = false;
  return document.activeElement === element;
}; // end attemptFocus

aria.handleEscape = function (event) {
  var key = event.which || event.keyCode;

  if (key === aria.KeyCode.ESC && aria.openedDialog) {
    aria.openedDialog.close();
    event.stopPropagation();
  }
};

document.addEventListener('keyup', aria.handleEscape);

/**
 * @class
 * @description Dialog object providing modal focus management.
 *
 * Assumptions: The element serving as the dialog container is present in the
 * DOM and hidden. The dialog container has role='dialog'.
 * @param dialogId
 *          The ID of the element serving as the dialog container.
 * @param focusAfterClosed
 *          Either the DOM node or the ID of the DOM node to focus when the
 *          dialog closes.
 * @param focusFirst
 *          Optional parameter containing either the DOM node or the ID of the
 *          DOM node to focus when the dialog opens. If not specified, the
 *          first focusable element in the dialog will receive focus.
 */
aria.Dialog = function (dialogId, focusAfterClosed, focusFirst) {
  this.dialogNode = document.getElementById(dialogId);
  if (this.dialogNode === null) {
    throw new Error('No element found with id="' + dialogId + '".');
  }

  var validRoles = ['dialog', 'alertdialog'];
  var isDialog = (this.dialogNode.getAttribute('role') || '')
    .trim()
    .split(/\s+/g)
    .some(function (token) {
      return validRoles.some(function (role) {
        return token === role;
      });
    });
  if (!isDialog) {
    throw new Error(
      'Dialog() requires a DOM element with ARIA role of dialog or alertdialog.'
    );
  }

  // Wrap in an individual backdrop element if one doesn't exist
  // Native <dialog> elements use the ::backdrop pseudo-element, which
  // works similarly.
  var backdropClass = 'dialog-backdrop';
  if (this.dialogNode.parentNode.classList.contains(backdropClass)) {
    this.backdropNode = this.dialogNode.parentNode;
  } else {
    this.backdropNode = document.createElement('div');
    this.backdropNode.className = backdropClass;
    this.dialogNode.parentNode.insertBefore(this.backdropNode, this.dialogNode);
    this.backdropNode.appendChild(this.dialogNode);
  }
  this.backdropNode.classList.add('active');

  // Disable scroll on the body element
  document.body.classList.add(aria.Utils.dialogOpenClass);

  if (typeof focusAfterClosed === 'string') {
    this.focusAfterClosed = document.getElementById(focusAfterClosed);
  } else if (typeof focusAfterClosed === 'object') {
    this.focusAfterClosed = focusAfterClosed;
  } else {
    throw new Error(
      'the focusAfterClosed parameter is required for the aria.Dialog constructor.'
    );
  }

  if (typeof focusFirst === 'string') {
    this.focusFirst = document.getElementById(focusFirst);
  } else if (typeof focusFirst === 'object') {
    this.focusFirst = focusFirst;
  } else {
    this.focusFirst = null;
  }

  // Bracket the dialog node with two invisible, focusable nodes.
  // While this dialog is open, we use these to make sure that focus never
  // leaves the document even if dialogNode is the first or last node.
  var preDiv = document.createElement('div');
  this.preNode = this.dialogNode.parentNode.insertBefore(
    preDiv,
    this.dialogNode
  );
  this.preNode.tabIndex = 0;
  var postDiv = document.createElement('div');
  this.postNode = this.dialogNode.parentNode.insertBefore(
    postDiv,
    this.dialogNode.nextSibling
  );
  this.postNode.tabIndex = 0;

  this.addListeners();
  aria.openedDialog = this;
  this.dialogNode.className = 'default_dialog'; // make visible

  if (this.focusFirst) {
    this.focusFirst.focus();
  } else {
    aria.Utils.focusFirstDescendant(this.dialogNode);
  }

  this.lastFocus = document.activeElement;
}; // end Dialog constructor

/**
 * @description
 *  Hides the current top dialog,
 *  removes listeners of the top dialog,
 *  restore listeners of a parent dialog if one was open under the one that just closed,
 *  and sets focus on the element specified for focusAfterClosed.
 */
aria.Dialog.prototype.close = function () {
  aria.openedDialog = null;
  this.removeListeners();
  aria.Utils.remove(this.preNode);
  aria.Utils.remove(this.postNode);
  this.dialogNode.className = 'hidden';
  this.backdropNode.classList.remove('active');
  this.focusAfterClosed.focus();

  document.body.classList.remove(aria.Utils.dialogOpenClass);
}; // end close

aria.Dialog.prototype.addListeners = function () {
  document.addEventListener('focus', this.trapFocus, true);
}; // end addListeners

aria.Dialog.prototype.removeListeners = function () {
  document.removeEventListener('focus', this.trapFocus, true);
}; // end removeListeners

aria.Dialog.prototype.trapFocus = function (event) {
  if (aria.Utils.IgnoreUtilFocusChanges) {
    return;
  }
  var opened = aria.openedDialog;
  if (opened.dialogNode.contains(event.target)) {
    opened.lastFocus = event.target;
  } else {
    aria.Utils.focusFirstDescendant(opened.dialogNode);
    if (opened.lastFocus == document.activeElement) {
      aria.Utils.focusLastDescendant(opened.dialogNode);
    }
    opened.lastFocus = document.activeElement;
  }
}; // end trapFocus

aria.Utils.disableCtrl = function (ctrl) {
  ctrl.setAttribute('aria-disabled', 'true');
};

aria.Utils.enableCtrl = function (ctrl) {
  ctrl.removeAttribute('aria-disabled');
};

aria.Utils.setLoading = function (saveBtn, saveStatusView) {
  saveBtn.classList.add('loading');
  this.disableCtrl(saveBtn);

  // use a timeout for the loading message
  // if the saved state happens very quickly,
  // we don't need to explicitly announce the intermediate loading state
  const loadingTimeout = window.setTimeout(() => {
    saveStatusView.textContent = 'Loading';
  }, 200);

  // set timeout for saved state, to mimic loading
  const fakeLoadingTimeout = Math.random() * 2000;
  window.setTimeout(() => {
    saveBtn.classList.remove('loading');
    saveBtn.classList.add('saved');

    window.clearTimeout(loadingTimeout);
    saveStatusView.textContent = 'Saved successfully';
  }, fakeLoadingTimeout);
};

aria.Notes = function Notes(
  notesId,
  saveId,
  saveStatusId,
  discardId,
  localStorageKey
) {
  this.notesInput = document.getElementById(notesId);
  this.saveBtn = document.getElementById(saveId);
  this.saveStatusView = document.getElementById(saveStatusId);
  this.discardBtn = document.getElementById(discardId);
  this.localStorageKey = localStorageKey || 'alertdialog-notes';
  this.initialized = false;

  Object.defineProperty(this, 'controls', {
    get: function () {
      return document.querySelectorAll(
        '[data-textbox=' + this.notesInput.id + ']'
      );
    },
  });
  Object.defineProperty(this, 'hasContent', {
    get: function () {
      return this.notesInput.value.length > 0;
    },
  });
  Object.defineProperty(this, 'savedValue', {
    get: function () {
      return JSON.parse(localStorage.getItem(this.localStorageKey));
    },
    set: function (val) {
      this.save(val);
    },
  });
  Object.defineProperty(this, 'isCurrent', {
    get: function () {
      return this.notesInput.value === this.savedValue;
    },
  });
  Object.defineProperty(this, 'oninput', {
    get: function () {
      return this.notesInput.oninput;
    },
    set: function (fn) {
      if (typeof fn !== 'function') {
        throw new TypeError('oninput must be a function');
      }
      this.notesInput.addEventListener('input', fn);
    },
  });

  if (this.saveBtn && this.discardBtn) {
    this.init();
  }
};

aria.Notes.prototype.save = function (val) {
  const isDisabled = this.saveBtn.getAttribute('aria-disabled') === 'true';
  if (isDisabled) {
    return;
  }
  localStorage.setItem(
    this.localStorageKey,
    JSON.stringify(val || this.notesInput.value)
  );
  aria.Utils.disableCtrl(this.saveBtn);
  aria.Utils.setLoading(this.saveBtn, this.saveStatusView);
};

aria.Notes.prototype.loadSaved = function () {
  if (this.savedValue) {
    this.notesInput.value = this.savedValue;
  }
};

aria.Notes.prototype.restoreSaveBtn = function () {
  this.saveBtn.classList.remove('loading');
  this.saveBtn.classList.remove('saved');
  this.saveBtn.removeAttribute('aria-disabled');

  this.saveStatusView.textContent = '';
};

aria.Notes.prototype.discard = function () {
  localStorage.clear();
  this.notesInput.value = '';
  this.toggleControls();
  this.restoreSaveBtn();
};

aria.Notes.prototype.disableControls = function () {
  this.controls.forEach(aria.Utils.disableCtrl);
};

aria.Notes.prototype.enableControls = function () {
  this.controls.forEach(aria.Utils.enableCtrl);
};

aria.Notes.prototype.toggleControls = function () {
  if (this.hasContent) {
    this.enableControls();
  } else {
    this.disableControls();
  }
};

aria.Notes.prototype.toggleCurrent = function () {
  if (!this.isCurrent) {
    this.notesInput.classList.remove('can-save');
    aria.Utils.enableCtrl(this.saveBtn);
    this.restoreSaveBtn();
  } else {
    this.notesInput.classList.add('can-save');
    aria.Utils.disableCtrl(this.saveBtn);
  }
};

aria.Notes.prototype.keydownHandler = function (e) {
  var mod = navigator.userAgent.includes('Mac') ? e.metaKey : e.ctrlKey;
  if ((e.key === 's') & mod) {
    e.preventDefault();
    this.save();
  }
};

aria.Notes.prototype.init = function () {
  if (!this.initialized) {
    this.loadSaved();
    this.toggleCurrent();
    this.saveBtn.addEventListener('click', this.save.bind(this, undefined));
    this.discardBtn.addEventListener('click', this.discard.bind(this));
    this.notesInput.addEventListener('input', this.toggleControls.bind(this));
    this.notesInput.addEventListener('input', this.toggleCurrent.bind(this));
    this.notesInput.addEventListener('keydown', this.keydownHandler.bind(this));
    this.initialized = true;
  }
};

/** initialization */
document.addEventListener('DOMContentLoaded', function initAlertDialog() {
  var notes = new aria.Notes(
    'notes',
    'notes_save',
    'notes_save_status',
    'notes_confirm'
  );

  window.closeDialog = function () {
    aria.openedDialog.close();
  }; // end closeDialog

  window.discardInput = function () {
    notes.discard.call(notes);
    window.closeDialog();
  };

  window.openAlertDialog = function (dialogId, triggerBtn, focusFirst) {
    // do not proceed if the trigger button is disabled
    if (triggerBtn.getAttribute('aria-disabled') === 'true') {
      return;
    }

    var target = document.getElementById(
      triggerBtn.getAttribute('data-textbox')
    );
    var dialog = document.getElementById(dialogId);
    var desc = document.getElementById(dialog.getAttribute('aria-describedby'));
    var wordCount = document.getElementById('word_count');
    if (!wordCount) {
      wordCount = document.createElement('p');
      wordCount.id = 'word_count';
      desc.appendChild(wordCount);
    }
    var count = target.value.split(/\s/).length;
    var frag = count > 1 ? 'words' : 'word';
    wordCount.textContent = count + ' ' + frag + ' will be deleted.';
    new aria.Dialog(dialogId, target, focusFirst);
  };
});
