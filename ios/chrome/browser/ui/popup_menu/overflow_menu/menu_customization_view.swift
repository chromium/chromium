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

  static let headerHeight: CGFloat = 56

  @ObservedObject var actionCustomizationModel: ActionCustomizationModel

  @ObservedObject var destinationCustomizationModel: DestinationCustomizationModel

  @ObservedObject var uiConfiguration: OverflowMenuUIConfiguration

  @StateObject var dragHandler: DestinationDragHandler

  weak var eventHandler: MenuCustomizationEventHandler?

  /// The namespace for the animation of this view appearing or disappearing.
  let namespace: Namespace.ID

  /// Focus state to allow setting VoiceOver focus to the page header when
  /// the page appears.
  @AccessibilityFocusState
  private var headerFocused: Bool

  init(
    actionCustomizationModel: ActionCustomizationModel,
    destinationCustomizationModel: DestinationCustomizationModel,
    uiConfiguration: OverflowMenuUIConfiguration,
    eventHandler: MenuCustomizationEventHandler?,
    namespace: Namespace.ID
  ) {
    self.actionCustomizationModel = actionCustomizationModel
    self.destinationCustomizationModel = destinationCustomizationModel
    self.uiConfiguration = uiConfiguration
    self.eventHandler = eventHandler
    self.namespace = namespace

    _dragHandler = StateObject(
      wrappedValue: DestinationDragHandler(destinationModel: destinationCustomizationModel))
  }

  var body: some View {
    GeometryReader { geometry in
      VStack(alignment: .leading, spacing: 0) {
        header
        OverflowMenuDestinationList(
          destinations: $destinationCustomizationModel.shownDestinations,
          width: geometry.size.width, metricsHandler: nil,
          uiConfiguration: uiConfiguration, dragHandler: dragHandler, namespace: namespace
        )
        .matchedGeometryEffect(id: MenuCustomizationAnimationID.destinations, in: namespace)
        if destinationCustomizationModel.hiddenDestinations.count > 0 {
          Text(
            L10nUtils.stringWithFixup(messageId: IDS_IOS_OVERFLOW_MENU_EDIT_SECTION_HIDDEN_TITLE)
          )
          .fontWeight(.semibold)
          .padding([.leading], Self.leadingPadding)
          .accessibilityAddTraits(.isHeader)
          OverflowMenuDestinationList(
            destinations: $destinationCustomizationModel.hiddenDestinations,
            width: geometry.size.width, metricsHandler: nil,
            uiConfiguration: uiConfiguration, namespace: namespace
          )
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
              Toggle(isOn: $destinationCustomizationModel.destinationUsageEnabled) {
                Text(L10nUtils.stringWithFixup(messageId: IDS_IOS_OVERFLOW_MENU_SORT_TITLE))
              }
              .labelsHidden()
              .tint(.chromeBlue)
            }
            .accessibilityElement(children: .combine)
          }
          OverflowMenuActionSection(
            actionGroup: actionCustomizationModel.actionsGroup, metricsHandler: nil
          )
        }
        .matchedGeometryEffect(id: MenuCustomizationAnimationID.actions, in: namespace)
      }
      .background(Color(.systemGroupedBackground).edgesIgnoringSafeArea(.all))
      .overflowMenuListStyle()
      .environment(\.editMode, .constant(.active))
      .onAppear {
        headerFocused = true
      }
    }
  }

  /// Custom header for this view. This should look like a `NavigationView`'s
  /// toolbar, but that can't be animated.
  @ViewBuilder
  var header: some View {
    HStack(alignment: .center, spacing: 0) {
      // The 3 nested HStacks mean that the space is divided equally between the
      // three. Specifically, this means that the middle HStack is centered in
      // the entire width, rather than centered between the two side buttons
      // (as they are different lengths).
      HStack {
        Button(
          L10nUtils.stringWithFixup(
            messageId: IDS_IOS_OVERFLOW_MENU_CUSTOMIZE_MENU_CANCEL)
        ) {
          eventHandler?.cancelWasTapped()
        }
        .padding([.leading])
        Spacer()
      }

      HStack {
        Text(
          L10nUtils.stringWithFixup(
            messageId: IDS_IOS_OVERFLOW_MENU_CUSTOMIZE_MENU_TITLE)
        )
        .fontWeight(.semibold)
        .lineLimit(1)
        .accessibilityFocused($headerFocused)
        .accessibilityAddTraits(.isHeader)
      }
      .layoutPriority(1000)

      HStack {
        Spacer()
        Button {
          eventHandler?.doneWasTapped()
        } label: {
          Text(
            L10nUtils.stringWithFixup(
              messageId: IDS_IOS_OVERFLOW_MENU_CUSTOMIZE_MENU_DONE)
          )
          .bold()
        }
        .disabled(
          !destinationCustomizationModel.hasChanged && !actionCustomizationModel.hasChanged
        )
        .padding([.trailing])
      }
    }
    .frame(minHeight: Self.headerHeight)
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
