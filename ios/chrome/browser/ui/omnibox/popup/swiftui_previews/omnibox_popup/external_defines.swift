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
