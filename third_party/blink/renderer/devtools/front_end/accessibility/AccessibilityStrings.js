// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Accessibility.AccessibilityStrings = {};

Accessibility.AccessibilityStrings.AXAttributes = {
  'disabled': {
    name: 'Disabled',
    description: 'If true, this element currently cannot be interacted with.',
    group: 'AXGlobalStates'
  },
  'invalid': {
    name: 'Invalid user entry',
    description: 'If true, this element\'s user-entered value does not conform to validation requirement.',
    group: 'AXGlobalStates'
  },
  'editable': {name: 'Editable', description: 'If and how this element can be edited.'},
  'focusable': {name: 'Focusable', description: 'If true, this element can recieve focus.'},
  'focused': {name: 'Focused', description: 'If true, this element currently has focus.'},
  'settable': {name: 'Can set value', description: 'Whether the value of this element can be set.'},
  'live': {
    name: 'Live region',
    description: 'Whether and what priority of live updates may be expected for this element.',
    group: 'AXLiveRegionAttributes'
  },
  'atomic': {
    name: 'Atomic (live regions)',
    description:
        'If this element may receive live updates, whether the entire live region should be presented to the user on changes, or only changed nodes.',
    group: 'AXLiveRegionAttributes'
  },
  'relevant': {
    name: 'Relevant (live regions)',
    description: 'If this element may receive live updates, what type of updates should trigger a notification.',
    group: 'AXLiveRegionAttributes'
  },
  'busy': {
    name: 'Busy (live regions)',
    description:
        'Whether this element or its subtree are currently being updated (and thus may be in an inconsistent state).',
    group: 'AXLiveRegionAttributes'
  },
  'root': {
    name: 'Live region root',
    description: 'If this element may receive live updates, the root element of the containing live region.',
    group: 'AXLiveRegionAttributes'
  },
  'autocomplete': {
    name: 'Has autocomplete',
    description: 'Whether and what type of autocomplete suggestions are currently provided by this element.',
    group: 'AXWidgetAttributes'
  },
  'haspopup': {
    name: 'Has popup',
    description: 'Whether this element has caused some kind of pop-up (such as a menu) to appear.',
    group: 'AXWidgetAttributes'
  },
  'level': {name: 'Level', description: 'The hierarchical level of this element.', group: 'AXWidgetAttributes'},
  'multiselectable': {
    name: 'Multi-selectable',
    description: 'Whether a user may select more than one option from this widget.',
    group: 'AXWidgetAttributes'
  },
  'orientation': {
    name: 'Orientation',
    description: 'Whether this linear element\'s orientation is horizontal or vertical.',
    group: 'AXWidgetAttributes'
  },
  'multiline': {
    name: 'Multi-line',
    description: 'Whether this textbox may have more than one line.',
    group: 'AXWidgetAttributes'
  },
  'readonly': {
    name: 'Read-only',
    description: 'If true, this element may be interacted with, but its value cannot be changed.',
    group: 'AXWidgetAttributes'
  },
  'required': {
    name: 'Required',
    description: 'Whether this element is a required field in a form.',
    group: 'AXWidgetAttributes'
  },
  'valuemin': {
    name: 'Minimum value',
    description: 'For a range widget, the minimum allowed value.',
    group: 'AXWidgetAttributes'
  },
  'valuemax': {
    name: 'Maximum value',
    description: 'For a range widget, the maximum allowed value.',
    group: 'AXWidgetAttributes'
  },
  'valuetext': {
    name: 'Value description',
    description: 'A human-readable version of the value of a range widget (where necessary).',
    group: 'AXWidgetAttributes'
  },
  'checked': {
    name: 'Checked',
    description:
        'Whether this checkbox, radio button or tree item is checked, unchecked, or mixed (e.g. has both checked and un-checked children).',
    group: 'AXWidgetStates'
  },
  'expanded': {
    name: 'Expanded',
    description: 'Whether this element, or another grouping element it controls, is expanded.',
    group: 'AXWidgetStates'
  },
  'pressed': {
    name: 'Pressed',
    description: 'Whether this toggle button is currently in a pressed state.',
    group: 'AXWidgetStates'
  },
  'selected': {
    name: 'Selected',
    description: 'Whether the option represented by this element is currently selected.',
    group: 'AXWidgetStates'
  },
  'activedescendant': {
    name: 'Active descendant',
    description: 'The descendant of this element which is active; i.e. the element to which focus should be delegated.',
    group: 'AXRelationshipAttributes'
  },
  'flowto': {
    name: 'Flows to',
    description:
        'Element to which the user may choose to navigate after this one, instead of the next element in the DOM order.',
    group: 'AXRelationshipAttributes'
  },
  'controls': {
    name: 'Controls',
    description: 'Element or elements whose content or presence is/are controlled by this widget.',
    group: 'AXRelationshipAttributes'
  },
  'describedby': {
    name: 'Described by',
    description: 'Element or elements which form the description of this element.',
    group: 'AXRelationshipAttributes'
  },
  'labelledby': {
    name: 'Labeled by',
    description: 'Element or elements which may form the name of this element.',
    group: 'AXRelationshipAttributes'
  },
  'owns': {
    name: 'Owns',
    description:
        'Element or elements which should be considered descendants of this element, despite not being descendants in the DOM.',
    group: 'AXRelationshipAttributes'
  },
  'name': {name: 'Name', description: 'The computed name of this element.', group: 'Default'},
  'role': {
    name: 'Role',
    description:
        'Indicates the purpose of this element, such as a user interface idiom for a widget, or structural role within a document.',
    group: 'Default'
  },
  'value': {
    name: 'Value',
    description:
        'The value of this element; this may be user-provided or developer-provided, depending on the element.',
    group: 'Default'
  },
  'help': {name: 'Help', description: 'The computed help text for this element.', group: 'Default'},
  'description': {name: 'Description', description: 'The accessible description for this element.', group: 'Default'}
};

Accessibility.AccessibilityStrings.AXSourceTypes = {
  'attribute': {name: 'From attribute', description: 'Value from attribute.'},
  'implicit': {
    name: 'Implicit',
    description: 'Implicit value.',
  },
  'style': {name: 'From style', description: 'Value from style.'},
  'contents': {name: 'Contents', description: 'Value from element contents.'},
  'placeholder': {name: 'From placeholder attribute', description: 'Value from placeholder attribute.'},
  'relatedElement': {name: 'Related element', description: 'Value from related element.'}
};

Accessibility.AccessibilityStrings.AXNativeSourceTypes = {
  'figcaption': {name: 'From caption', description: 'Value from figcaption element.'},
  'label': {name: 'From label', description: 'Value from label element.'},
  'labelfor': {name: 'From label (for)', description: 'Value from label element with for= attribute.'},
  'labelwrapped': {name: 'From label (wrapped)', description: 'Value from label element wrapped.'},
  'tablecaption': {name: 'From caption', description: 'Value from table caption.'},
  'title': {'name': 'From title', 'description': 'Value from title attribute.'},
  'other': {name: 'From native HTML', description: 'Value from native HTML (unknown source).'},

};
