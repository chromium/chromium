/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   Supplemental JS for the disclosure menu keyboard behavior
 */

'use strict';

class DisclosureNav {
  constructor(domNode) {
    this.rootNode = domNode;
    this.controlledNodes = [];
    this.openIndex = null;
    this.useArrowKeys = true;
    this.topLevelNodes = [
      ...this.rootNode.querySelectorAll(
        '.main-link, button[aria-expanded][aria-controls]'
      ),
    ];

    this.topLevelNodes.forEach((node) => {
      // handle button + menu
      if (
        node.tagName.toLowerCase() === 'button' &&
        node.hasAttribute('aria-controls')
      ) {
        const menu = node.parentNode.querySelector('ul');
        if (menu) {
          // save ref controlled menu
          this.controlledNodes.push(menu);

          // collapse menus
          node.setAttribute('aria-expanded', 'false');
          this.toggleMenu(menu, false);

          // attach event listeners
          menu.addEventListener('keydown', this.onMenuKeyDown.bind(this));
          node.addEventListener('click', this.onButtonClick.bind(this));
          node.addEventListener('keydown', this.onButtonKeyDown.bind(this));
        }
      }
      // handle links
      else {
        this.controlledNodes.push(null);
        node.addEventListener('keydown', this.onLinkKeyDown.bind(this));
      }
    });

    this.rootNode.addEventListener('focusout', this.onBlur.bind(this));
  }

  controlFocusByKey(keyboardEvent, nodeList, currentIndex) {
    switch (keyboardEvent.key) {
      case 'ArrowUp':
      case 'ArrowLeft':
        keyboardEvent.preventDefault();
        if (currentIndex > -1) {
          var prevIndex = Math.max(0, currentIndex - 1);
          nodeList[prevIndex].focus();
        }
        break;
      case 'ArrowDown':
      case 'ArrowRight':
        keyboardEvent.preventDefault();
        if (currentIndex > -1) {
          var nextIndex = Math.min(nodeList.length - 1, currentIndex + 1);
          nodeList[nextIndex].focus();
        }
        break;
      case 'Home':
        keyboardEvent.preventDefault();
        nodeList[0].focus();
        break;
      case 'End':
        keyboardEvent.preventDefault();
        nodeList[nodeList.length - 1].focus();
        break;
    }
  }

  // public function to close open menu
  close() {
    this.toggleExpand(this.openIndex, false);
  }

  onBlur(event) {
    var menuContainsFocus = this.rootNode.contains(event.relatedTarget);
    if (!menuContainsFocus && this.openIndex !== null) {
      this.toggleExpand(this.openIndex, false);
    }
  }

  onButtonClick(event) {
    var button = event.target;
    var buttonIndex = this.topLevelNodes.indexOf(button);
    var buttonExpanded = button.getAttribute('aria-expanded') === 'true';
    this.toggleExpand(buttonIndex, !buttonExpanded);
  }

  onButtonKeyDown(event) {
    var targetButtonIndex = this.topLevelNodes.indexOf(document.activeElement);

    // close on escape
    if (event.key === 'Escape') {
      this.toggleExpand(this.openIndex, false);
    }

    // move focus into the open menu if the current menu is open
    else if (
      this.useArrowKeys &&
      this.openIndex === targetButtonIndex &&
      event.key === 'ArrowDown'
    ) {
      event.preventDefault();
      this.controlledNodes[this.openIndex].querySelector('a').focus();
    }

    // handle arrow key navigation between top-level buttons, if set
    else if (this.useArrowKeys) {
      this.controlFocusByKey(event, this.topLevelNodes, targetButtonIndex);
    }
  }

  onLinkKeyDown(event) {
    var targetLinkIndex = this.topLevelNodes.indexOf(document.activeElement);

    // handle arrow key navigation between top-level buttons, if set
    if (this.useArrowKeys) {
      this.controlFocusByKey(event, this.topLevelNodes, targetLinkIndex);
    }
  }

  onMenuKeyDown(event) {
    if (this.openIndex === null) {
      return;
    }

    var menuLinks = Array.prototype.slice.call(
      this.controlledNodes[this.openIndex].querySelectorAll('a')
    );
    var currentIndex = menuLinks.indexOf(document.activeElement);

    // close on escape
    if (event.key === 'Escape') {
      this.topLevelNodes[this.openIndex].focus();
      this.toggleExpand(this.openIndex, false);
    }

    // handle arrow key navigation within menu links, if set
    else if (this.useArrowKeys) {
      this.controlFocusByKey(event, menuLinks, currentIndex);
    }
  }

  toggleExpand(index, expanded) {
    // close open menu, if applicable
    if (this.openIndex !== index) {
      this.toggleExpand(this.openIndex, false);
    }

    // handle menu at called index
    if (this.topLevelNodes[index]) {
      this.openIndex = expanded ? index : null;
      this.topLevelNodes[index].setAttribute('aria-expanded', expanded);
      this.toggleMenu(this.controlledNodes[index], expanded);
    }
  }

  toggleMenu(domNode, show) {
    if (domNode) {
      domNode.style.display = show ? 'block' : 'none';
    }
  }

  updateKeyControls(useArrowKeys) {
    this.useArrowKeys = useArrowKeys;
  }
}

/* Initialize Disclosure Menus */

window.addEventListener(
  'load',
  function () {
    var menus = document.querySelectorAll('.disclosure-nav');
    var disclosureMenus = [];

    for (var i = 0; i < menus.length; i++) {
      disclosureMenus[i] = new DisclosureNav(menus[i]);
    }

    // listen to arrow key checkbox
    var arrowKeySwitch = document.getElementById('arrow-behavior-switch');
    if (arrowKeySwitch) {
      arrowKeySwitch.addEventListener('change', function () {
        var checked = arrowKeySwitch.checked;
        for (var i = 0; i < disclosureMenus.length; i++) {
          disclosureMenus[i].updateKeyControls(checked);
        }
      });
    }

    // fake link behavior
    disclosureMenus.forEach((disclosureNav, i) => {
      var links = menus[i].querySelectorAll('[href="#mythical-page-content"]');
      var examplePageHeading = document.getElementById('mythical-page-heading');
      for (var k = 0; k < links.length; k++) {
        // The codepen export script updates the internal link href with a full URL
        // we're just manually fixing that behavior here
        links[k].href = '#mythical-page-content';

        links[k].addEventListener('click', (event) => {
          // change the heading text to fake a page change
          var pageTitle = event.target.innerText;
          examplePageHeading.innerText = pageTitle;

          // handle aria-current
          for (var n = 0; n < links.length; n++) {
            links[n].removeAttribute('aria-current');
          }
          event.target.setAttribute('aria-current', 'page');
        });
      }
    });
  },
  false
);
