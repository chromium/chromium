//
//  external_defines.swift
//  omnibox_popup
//
//  Created by Stepan Khapugin on 31/03/2022.
//

import Foundation

// Defines for chrome constants etc that aren't built as part of this demo project.

public let kOmniboxPopupRowSwitchTabAccessibilityIdentifier =
  "kOmniboxPopupRowSwitchTabAccessibilityIdentifier"

public let kOmniboxPopupRowAppendAccessibilityIdentifier =
  "kOmniboxPopupRowAppendAccessibilityIdentifier"

public let kOmniboxPopupTableViewAccessibilityIdentifier =
  "OmniboxPopupTableViewAccessibilityIdentifier"

public class OmniboxPopupAccessibilityIdentifierHelper {
  static func accessibilityIdentifierForRow(at indexPath: IndexPath) -> String {
    return "omnibox suggestion \(indexPath.section) \(indexPath.row)"
  }
}

// These constants are needed for the previews project, but named in C++ code,
// so they shouldn't be linted.
// swift-format-ignore: AlwaysUseLowerCamelCase
let IDS_IOS_OMNIBOX_POPUP_SWITCH_TO_OPEN_TAB = 1
// swift-format-ignore: AlwaysUseLowerCamelCase
let IDS_IOS_OMNIBOX_POPUP_APPEND = 2
// swift-format-ignore: AlwaysUseLowerCamelCase
let IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND = 3

public class L10NUtils {
  public static func string(forMessageId: Int) -> String? {
    switch forMessageId {
    case IDS_IOS_OMNIBOX_POPUP_SWITCH_TO_OPEN_TAB:
      return "Open tab"

    case IDS_IOS_OMNIBOX_POPUP_APPEND:
      return "Append"

    default:
      return "STRING_NOT_DEFINED"

    }
  }
}

public func NativeImage(_ imageID: Int) -> UIImage? {
  switch imageID {
  case IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND:
    let uiImage = UIImage(named: "IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND")
    return uiImage
  default:
    fatalError("This image ID is not available in the previews project")
  }
}

let kToolbarSeparatorHeight = 0.1
let kContractedLocationBarHorizontalMargin = 15.0
let kExpandedLocationBarLeadingMarginRefreshedPopup = 16.0
