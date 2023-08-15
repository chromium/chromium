// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

@objc public protocol MenuCustomizationEventHandler: AnyObject {
  func cancelWasTapped()
  func doneWasTapped()
}

/// View for showing the customization screen for overflow menu
struct MenuCustomizationView: View {
  /// Leading padding for any views that require it.
  static let leadingPadding: CGFloat = 16

  @ObservedObject var actionCustomizationModel: ActionCustomizationModel

  @ObservedObject var destinationCustomizationModel: DestinationCustomizationModel

  @ObservedObject var uiConfiguration: OverflowMenuUIConfiguration

  @StateObject var dragHandler: DestinationDragHandler

  weak var eventHandler: MenuCustomizationEventHandler?

  init(
    actionCustomizationModel: ActionCustomizationModel,
    destinationCustomizationModel: DestinationCustomizationModel,
    uiConfiguration: OverflowMenuUIConfiguration,
    eventHandler: MenuCustomizationEventHandler
  ) {
    self.actionCustomizationModel = actionCustomizationModel
    self.destinationCustomizationModel = destinationCustomizationModel
    self.uiConfiguration = uiConfiguration
    self.eventHandler = eventHandler

    _dragHandler = StateObject(
      wrappedValue: DestinationDragHandler(destinationModel: destinationCustomizationModel))
  }

  var body: some View {
    NavigationView {
      VStack(alignment: .leading, spacing: 0) {
        OverflowMenuDestinationList(
          destinations: $destinationCustomizationModel.shownDestinations, metricsHandler: nil,
          uiConfiguration: uiConfiguration, dragHandler: dragHandler
        ).frame(height: OverflowMenuListStyle.destinationListHeight)
        if destinationCustomizationModel.hiddenDestinations.count > 0 {
          Text(
            L10nUtils.stringWithFixup(messageId: IDS_IOS_OVERFLOW_MENU_EDIT_SECTION_HIDDEN_TITLE)
          )
          .fontWeight(.semibold)
          .padding([.leading], Self.leadingPadding)
          OverflowMenuDestinationList(
            destinations: $destinationCustomizationModel.hiddenDestinations, metricsHandler: nil,
            uiConfiguration: uiConfiguration
          ).frame(height: OverflowMenuListStyle.destinationListHeight)
        }
        Divider()
        List {
          createDefaultSection {
            HStack {
              VStack(alignment: .leading) {
                Text(L10nUtils.stringWithFixup(messageId: IDS_IOS_OVERFLOW_MENU_SORT_TITLE))
                Text(L10nUtils.stringWithFixup(messageId: IDS_IOS_OVERFLOW_MENU_SORT_DESCRIPTION))
                  .font(.caption)
              }
              Spacer()
              Toggle(isOn: $destinationCustomizationModel.destinationUsageEnabled) {}
                .labelsHidden()
                .tint(.chromeBlue)
            }
          }
          ForEach([actionCustomizationModel.shownActions, actionCustomizationModel.hiddenActions]) {
            group in
            OverflowMenuActionSection(actionGroup: group, metricsHandler: nil)
          }
        }
      }
      .background(Color(.systemGroupedBackground).edgesIgnoringSafeArea(.all))
      .overflowMenuListStyle()
      .environment(\.editMode, .constant(.active))
      .navigationTitle(
        L10nUtils.stringWithFixup(
          messageId: IDS_IOS_OVERFLOW_MENU_CUSTOMIZE_MENU_TITLE)
      )
      .navigationBarTitleDisplayMode(.inline)
      .toolbar {
        ToolbarItem(placement: .cancellationAction) {
          Button(
            L10nUtils.stringWithFixup(
              messageId: IDS_IOS_OVERFLOW_MENU_CUSTOMIZE_MENU_CANCEL)
          ) {
            eventHandler?.cancelWasTapped()
          }
        }
        ToolbarItem(placement: .confirmationAction) {
          Button(
            L10nUtils.stringWithFixup(
              messageId: IDS_IOS_OVERFLOW_MENU_CUSTOMIZE_MENU_DONE)
          ) {
            eventHandler?.doneWasTapped()
          }.disabled(
            !destinationCustomizationModel.hasChanged && !actionCustomizationModel.hasChanged)
        }
      }
    }
  }

  /// Creates a section with default spacing in the header and footer.
  func createDefaultSection<SectionContent: View>(content: () -> SectionContent) -> some View {
    Section(
      content: content,
      header: {
        Spacer()
          .frame(height: OverflowMenuListStyle.headerFooterHeight)
          .listRowInsets(EdgeInsets())
          .accessibilityHidden(true)
      },
      footer: {
        Spacer()
          // Use `leastNonzeroMagnitude` to remove the footer. Otherwise,
          // it uses a default height.
          .frame(height: CGFloat.leastNonzeroMagnitude)
          .listRowInsets(EdgeInsets())
          .accessibilityHidden(true)
      })
  }
}
