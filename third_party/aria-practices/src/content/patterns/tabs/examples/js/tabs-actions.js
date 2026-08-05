/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   tabs-actions.js
 *
 *   Desc:   Tablist widget that implements ARIA Authoring Practices
 */

'use strict';

class TabsManual {
  constructor(groupNode) {
    this.tablistNode = groupNode;

    this.tabs = [];

    this.firstTab = null;
    this.lastTab = null;

    this.tabs = Array.from(this.tablistNode.querySelectorAll('[role=tab]'));
    this.tabpanels = [];

    for (var i = 0; i < this.tabs.length; i += 1) {
      var tab = this.tabs[i];
      var tabpanel = document.getElementById(tab.getAttribute('aria-controls'));

      tab.tabIndex = -1;
      tab.setAttribute('aria-selected', 'false');
      this.tabpanels.push(tabpanel);

      tab.addEventListener('keydown', this.onKeydown.bind(this));
      tab.addEventListener('click', this.onClick.bind(this));
      tab.addEventListener('focusout', this.onFocusout.bind(this));

      this.getTabAriaActions(tab).forEach((action) => {
        action.addEventListener('keydown', this.onKeydown.bind(this));
        action.addEventListener('focusout', this.onFocusout.bind(this));

        this.getTabAriaActionOperations(action).forEach((operation) => {
          operation.addEventListener(
            'click',
            this.performTabOperation.bind(this)
          );
          operation.addEventListener(
            'keydown',
            this.performTabOperation.bind(this)
          );
        });
      });

      if (!this.firstTab) {
        this.firstTab = tab;
      }
      this.lastTab = tab;
    }

    this.setSelectedTab(this.firstTab);
  }

  getTabAriaActions(tab) {
    var actions = tab.getAttribute('aria-actions');
    if (actions) {
      return actions.split(' ').map((id) => document.getElementById(id));
    } else {
      return [];
    }
  }

  getTabAriaActionOperations(action) {
    const operationCode = action.getAttribute('data-operation');
    const idrefControls = action.getAttribute('aria-controls');
    let operations = [];
    if (operationCode) {
      operations.push(action);
    }
    if (idrefControls) {
      const context = document.getElementById(idrefControls);
      const nodes = Array.from(context.querySelectorAll('[data-operation]'));
      operations = operations.concat(nodes);
    }
    return operations;
  }

  getTabAssociatedWithAction(action) {
    return document.querySelector(`[aria-actions~="${action.id}"]`);
  }

  getClosestTabWrapper(el) {
    return el ? el.closest('.tab-wrapper') : null;
  }

  getTabpanelAssociatedWithTab(tab) {
    return document.getElementById(tab.getAttribute('aria-controls'));
  }

  getActionAssociatedWithOperation(operation) {
    const idrefAction = operation.getAttribute('data-action');
    if (idrefAction) {
      return document.getElementById(idrefAction);
    } else {
      return operation;
    }
  }

  makeTabAndActionsFocusable(tab) {
    tab.removeAttribute('tabindex');
    this.getTabAriaActions(tab).forEach((action) => {
      action.removeAttribute('tabindex');
    });
  }

  makeTabAndActionsUnfocusable(tab) {
    const selectedTab = this.getSelectedTab();
    if (tab !== selectedTab) {
      tab.tabIndex = -1;
      this.getTabAriaActions(tab).forEach((action) => {
        action.tabIndex = -1;
      });
    }
  }

  setSelectedTab(currentTab) {
    for (var i = 0; i < this.tabs.length; i += 1) {
      var tab = this.tabs[i];
      if (currentTab === tab) {
        tab.setAttribute('aria-selected', 'true');
        this.makeTabAndActionsFocusable(tab);
        this.tabpanels[i].classList.remove('is-hidden');
      } else {
        tab.setAttribute('aria-selected', 'false');
        this.makeTabAndActionsUnfocusable(tab);
        this.tabpanels[i].classList.add('is-hidden');
      }
    }
  }

  getSelectedTab() {
    return this.tablistNode.querySelector('[aria-selected="true"]');
  }

  moveFocusToTab(newTab) {
    const selectedTab = this.getSelectedTab();
    newTab.focus();
    this.tabs.forEach((tab) => {
      if (newTab === tab) {
        this.makeTabAndActionsFocusable(tab);
      } else if (tab !== selectedTab) {
        this.makeTabAndActionsUnfocusable(tab);
      }
    });
    return newTab;
  }

  moveFocusToPreviousTab(currentTab) {
    var newTab;
    if (currentTab === this.firstTab) {
      newTab = this.lastTab;
    } else {
      newTab = this.tabs[this.tabs.indexOf(currentTab) - 1];
    }
    return this.moveFocusToTab(newTab);
  }

  moveFocusToNextTab(currentTab) {
    var newTab;
    if (currentTab === this.lastTab) {
      newTab = this.firstTab;
    } else {
      newTab = this.tabs[this.tabs.indexOf(currentTab) + 1];
    }
    return this.moveFocusToTab(newTab);
  }

  copyTabpanelToClipboard(tab, output) {
    const tabPanel = document.getElementById(tab.getAttribute('aria-controls'));
    const tabText = tabPanel.textContent.replace(/\s+/g, ' ').trim();
    navigator.clipboard.writeText(tabText).then(
      () => {
        this.showSuccess(
          output,
          `Copied ${tab.textContent} tab contents to clipboard`
        );
      },
      (err) => {
        this.showError(
          output,
          `Failed to copy ${tab.textContent} tab contents to clipboard`,
          err
        );
      }
    );
  }

  relocateTab(tab, dir) {
    const index = this.tabs.indexOf(tab);
    const tabWrapper = this.getClosestTabWrapper(tab);
    const tabPanel = this.getTabpanelAssociatedWithTab(tab);
    const movementFunction = dir === 'forward' ? 'after' : 'before';
    const dest =
      dir === 'forward' ? 'nextElementSibling' : 'previousElementSibling';
    const destIndex = dir === 'forward' ? index + 1 : index - 1;
    const destTabWrapper = tabWrapper[dest];
    const destTabPanel = tabPanel[dest];

    if (destTabWrapper && destTabPanel) {
      destTabWrapper[movementFunction](tabWrapper);
      destTabPanel[movementFunction](tabPanel);
      this.tabs.splice(destIndex, 0, this.tabs.splice(index, 1)[0]);
      this.tabpanels.splice(destIndex, 0, this.tabpanels.splice(index, 1)[0]);
      this.firstTab = this.tabs[0];
      this.lastTab = this.tabs[this.tabs.length - 1];
      return true;
    } else {
      return false;
    }
  }

  deleteTab(tab) {
    if (this.tabs.length === 1) {
      return false;
    }

    const tabWrapper = this.getClosestTabWrapper(tab);
    const tabPanel = document.getElementById(tab.getAttribute('aria-controls'));
    const selectedTab = this.getSelectedTab();

    if (tab === selectedTab) {
      const newTab =
        tab === this.lastTab
          ? this.moveFocusToPreviousTab(selectedTab)
          : this.moveFocusToNextTab(selectedTab);
      this.setSelectedTab(newTab);
    } else {
      this.moveFocusToTab(selectedTab);
    }

    this.tabs = this.tabs.filter((t) => t !== tab);
    this.tabpanels = this.tabpanels.filter((tp) => tp !== tabPanel);
    this.firstTab = this.tabs[0];
    this.lastTab = this.tabs[this.tabs.length - 1];
    tabWrapper.remove();
    tabPanel.remove();

    return true;
  }

  /* EVENT HANDLERS */

  onKeydown(event) {
    var tgt = event.currentTarget,
      flag = false;

    if (tgt.getAttribute('role') !== 'tab') {
      tgt = this.getTabAssociatedWithAction(tgt);
    }

    switch (event.key) {
      case 'ArrowLeft':
        this.moveFocusToPreviousTab(tgt);
        flag = true;
        break;

      case 'ArrowRight':
        this.moveFocusToNextTab(tgt);
        flag = true;
        break;

      case 'Home':
        this.moveFocusToTab(this.firstTab);
        flag = true;
        break;

      case 'End':
        this.moveFocusToTab(this.lastTab);
        flag = true;
        break;

      default:
        break;
    }

    if (flag) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  // Since this example uses buttons for the tabs, the click onr also is activated
  // with the space and enter keys
  onClick(event) {
    this.setSelectedTab(event.currentTarget);
  }

  performTabOperation(event) {
    if (event.type === 'keydown' && event.code !== 'Enter') {
      return;
    }

    const operation = event.currentTarget;
    const action = this.getActionAssociatedWithOperation(operation);
    const tab = this.getTabAssociatedWithAction(action);
    const operationCode = operation.getAttribute('data-operation');
    const output = operation.closest('.tabs').querySelector('output');
    const tabName = tab.textContent.trim();

    switch (operationCode) {
      case 'clipboard-copy': {
        this.copyTabpanelToClipboard(tab, output);
        break;
      }
      case 'move-backward':
      case 'move-forward': {
        const dir = operationCode === 'move-forward' ? 'forward' : 'backward';
        if (this.relocateTab(tab, dir)) {
          this.showSuccess(output, `Moved ${tabName} ${dir}.`);
        } else {
          this.showError(output, `Can’t move ${tabName} ${dir} any further.`);
        }
        break;
      }
      case 'close': {
        if (this.deleteTab(tab)) {
          this.showSuccess(output, `Closed ${tabName}.`);
        } else {
          this.showError(output, 'Can’t delete the last tab.');
        }
        break;
      }
      default: {
        this.showError(output, `Sorry, ${operationCode} isn’t built yet.`);
      }
    }
  }

  showSuccess(output, msg) {
    if (output) {
      output.innerHTML = `<span class="success">${msg}</span>`;
    }
  }

  showError(output, msg) {
    if (output) {
      output.innerHTML = `<span class="error"><em>${msg}</em></span>`;
    }
  }

  onFocusout(event) {
    const tabWrapperOld = this.getClosestTabWrapper(event.currentTarget);
    const tabWrapperNew = this.getClosestTabWrapper(event.relatedTarget);
    if (tabWrapperOld !== tabWrapperNew) {
      const tab = tabWrapperOld.querySelector('[role="tab"]');
      this.makeTabAndActionsUnfocusable(tab);
    }
  }
}

// Initialize tablist

window.addEventListener('load', function () {
  var tablists = document.querySelectorAll('[role=tablist].manual');
  for (var i = 0; i < tablists.length; i++) {
    new TabsManual(tablists[i]);
  }
});
