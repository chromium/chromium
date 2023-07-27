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
      VStack {
        OverflowMenuDestinationList(
          destinations: $destinationCustomizationModel.shownDestinations, metricsHandler: nil,
          uiConfiguration: uiConfiguration, dragHandler: dragHandler
        ).frame(height: OverflowMenuListStyle.destinationListHeight)
        Divider()
        List {
          createDefaultSection {
            HStack {
              VStack(alignment: .leading) {
                Text("")
                Text("")
                  .font(.caption)
              }
              Toggle(isOn: $destinationCustomizationModel.destinationUsageEnabled) {}
                .labelsHidden()
                .tint(.chromeBlue)
            }
          }
          createDefaultSection {
            hiddenDestinationsContent
          }
          ForEach([actionCustomizationModel.shownActions, actionCustomizationModel.hiddenActions]) {
            group in
            OverflowMenuActionSection(actionGroup: group, metricsHandler: nil)
          }
        }
      }
      .onDrop(of: [.text], delegate: dragHandler.newDestinationListDropDelegate())
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

  /// Content of the section for hidden destinations. This is a drop target if
  /// there are no hidden destinations and a list of the hidden destinations
  /// otherwise.
  @ViewBuilder
  var hiddenDestinationsContent: some View {
    if destinationCustomizationModel.hiddenDestinations.isEmpty {
      // If the section is empty, then nothing can be dropped on it, so create a
      // fake single entry to act as a drop target for now.
      ForEach([1], id: \.self) { _ in
        ZStack {
          Spacer()
            .frame(height: 62)
          Text("")
        }
      }
      .onInsert(of: [.text], perform: dragHandler.performListDrop(index:providers:))
    } else {
      ForEach(destinationCustomizationModel.hiddenDestinations) { destination in
        // TODO(crbug.com/1463959): Replace with full row.
        Text(destination.name)
      }
      .onInsert(of: [.text], perform: dragHandler.performListDrop(index:providers:))
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
