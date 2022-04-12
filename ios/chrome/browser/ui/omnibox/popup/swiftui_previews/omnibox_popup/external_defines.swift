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

let IDS_IOS_OMNIBOX_POPUP_SWITCH_TO_OPEN_TAB = 1
let IDS_IOS_OMNIBOX_POPUP_APPEND = 2

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
